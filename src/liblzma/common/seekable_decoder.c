// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       seekable_decoder.c
/// \brief      Decodes .xz files and supports seeking
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "block_decoder.h"
#include "index.h"


typedef struct {
	enum {
		SEQ_ITER_NEXT_BLOCK,
		SEQ_BLOCK_HEADER,
		SEQ_BLOCK_INIT,
		SEQ_BLOCK_SKIP,
		SEQ_BLOCK_OUTPUT,
	} sequence;

	/// File info decoder or Block decoder
	lzma_next_coder next;

	/// Block options decoded by the Block Header decoder and used by
	/// the Block decoder.
	lzma_block block_options;

	/// Memory usage limit
	uint64_t memlimit;

	/// Amount of memory actually needed (only an estimate)
	uint64_t memusage;

	/// If true, we will tell the Block decoder to skip calculating
	/// and verifying the integrity check.
	bool ignore_check;

	/// Pointer to lzma_stream.seek_pos:
	///   - This is written to pass the target input seek position to
	///     the application when we return LZMA_SEEK_NEEDED.
	///   - An application-provided value is read from this when action
	///     is LZMA_SEEK_TO_OFFSET or LZMA_SEEK_TO_BLOCK.
	uint64_t *external_seek_pos;

	/// Iterator to the application-provided lzma_index.
	/// The application must keep the lzma_index available
	/// as long as it's using the seekable .xz decoder.
	lzma_index_iter iter;

	/// Current read position in the input file
	uint64_t cur_in_offset;

	/// Current uncompressed position in the file
	uint64_t cur_out_offset;

	/// Number of uncompressed bytes to skip in SEQ_BLOCK_SKIP
	/// to reach the requested target offset.
	uint64_t skip_out;

	/// Write position in buffer[]
	size_t buffer_pos;

	/// Buffer to hold Block Header in SEQ_BLOCK_HEADER, and
	/// uncompressed data to be discarded in SEQ_BLOCK_SKIP.
	uint8_t buffer[my_max(LZMA_BLOCK_HEADER_SIZE_MAX, 4096)];
} lzma_seekable_coder;


/// \brief      Seek input position to the beginning of a Block
///
/// \return     LZMA_OK if it was enough to adjust *in_pos.
///             LZMA_SEEK_NEEDED if input seeking is required.
static lzma_ret
seekable_seek_input(lzma_seekable_coder *coder,
		size_t *restrict in_pos, size_t in_size)
{
	const uint64_t old_off = coder->cur_in_offset;
	const uint64_t new_off = coder->iter.block.compressed_file_offset;
	coder->cur_in_offset = new_off;

	// When starting to decode a Block Header, buffer_pos starts at 0.
	// We can get here with any coder->sequence, so we always reset
	// buffer_pos here.
	coder->buffer_pos = 0;
	coder->sequence = SEQ_BLOCK_HEADER;

	if (new_off >= old_off && new_off - old_off <= in_size - *in_pos) {
		// The new input offset is in the current input buffer
		// or it's the very next byte after it. There's no
		// need to seek; it's enough to advance *in_pos.
		*in_pos += new_off - old_off;
		return LZMA_OK;
	}

	// The new input offset isn't available in the current
	// input buffer. We need to seek.
	*coder->external_seek_pos = new_off;
	return LZMA_SEEK_NEEDED;
}


/// \brief      Seek to the requested uncompressed offset or Block number
///
/// \return     LZMA_OK if no input seeking is needed. LZMA_SEEK_NEEDED if
///             input seeking is required. LZMA_SEEK_ERROR if the target
///             offset or Block doesn't exist in the .xz file.
static lzma_ret
seekable_seek_output(lzma_seekable_coder *coder,
		size_t *restrict in_pos, size_t in_size, lzma_action action)
{
	assert(action == LZMA_SEEK_TO_OFFSET || action == LZMA_SEEK_TO_BLOCK);

	const uint64_t target = *coder->external_seek_pos;

	if (action == LZMA_SEEK_TO_OFFSET) {
		// Don't call locate if we already have the right Block.
		//
		// NOTE: SEQ_ITER_NEXT_BLOCK means that we are seeking
		// right after the decoder initialization, and thus
		// the iterator doesn't contain valid data yet.
		if (coder->sequence != SEQ_ITER_NEXT_BLOCK
				&& coder->cur_out_offset <= target
				&& target
				< coder->iter.block.uncompressed_file_offset
				+ coder->iter.block.uncompressed_size) {
			// The new offset is within the current Block.
			// We might need to skip output bytes to reach it.
			coder->skip_out = target - coder->cur_out_offset;

			// We might not have decoded the Block Header yet,
			// so only change coder->sequence when it's already
			// past SEQ_BLOCK_SKIP.
			if (coder->sequence == SEQ_BLOCK_OUTPUT)
				coder->sequence = SEQ_BLOCK_SKIP;

			return LZMA_OK;
		}

		if (lzma_index_iter_locate(&coder->iter, target))
			return LZMA_SEEK_ERROR;

		assert(coder->iter.block.uncompressed_file_offset <= target);

		// The decoding will start at the beginning of the Block.
		// Determine out how many bytes we need to ignore until
		// we reach the uncompressed target offset.
		coder->skip_out = target
				- coder->iter.block.uncompressed_file_offset;

	} else {
		assert(action == LZMA_SEEK_TO_BLOCK);

		// Check if we already are at the beginning of
		// the target Block.
		if (coder->sequence != SEQ_ITER_NEXT_BLOCK
				&& target == coder->iter.block.number_in_file
				&& coder->cur_out_offset == coder->iter.block
					.uncompressed_file_offset) {
			// skip_out could be non-zero due to a previous seek.
			// Reset it so that we won't skip any bytes from
			// the beginning of the Block.
			coder->skip_out = 0;
			return LZMA_OK;
		}

		if (lzma_index_iter_locate_block(&coder->iter, target))
			return LZMA_SEEK_ERROR;

		assert(coder->iter.block.number_in_file == target);

		// When seeking to a beginning of a Block, no bytes are
		// skipped from the beginning of the Block.
		//
		// NOTE: Don't do this earlier because we don't want to
		// change the decoder state on LZMA_SEEK_ERROR.
		coder->skip_out = 0;
	}

	// If we get here, on the next lzma_code() call, the input and
	// output offsets will be at the beginning of the Block.
	//
	// If LZMA_SEEK_TO_OFFSET was used, cur_out_offset might be
	// incremented in SEQ_BLOCK_SKIP to reach the target offset.
	coder->cur_out_offset = coder->iter.block.uncompressed_file_offset;

	// Adjust *in_pos or tell the application to seek the input.
	return seekable_seek_input(coder, in_pos, in_size);
}


static lzma_ret
seekable_decode(void *coder_ptr, const lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	lzma_seekable_coder *coder = coder_ptr;

	if (action == LZMA_SEEK_TO_OFFSET || action == LZMA_SEEK_TO_BLOCK) {
		// LZMA_SEEK_NEEDED isn't an error but it's not LZMA_OK
		// so this macro does the right thing.
		return_if_error(
			seekable_seek_output(coder, in_pos, in_size, action));
	}

	// When decoding the actual Block, it may be able to produce more
	// output even if we don't give it any new input.
	while (true)
	switch (coder->sequence) {
	case SEQ_ITER_NEXT_BLOCK: {
		if (lzma_index_iter_next(&coder->iter,
				LZMA_INDEX_ITER_BLOCK)) {
			// The .xz file has no Blocks.
			return LZMA_STREAM_END;
		}

		return_if_error(seekable_seek_input(coder, in_pos, in_size));

		// seekable_seek_input() always this.
		assert(coder->sequence == SEQ_BLOCK_HEADER);
		FALLTHROUGH;
	}

	case SEQ_BLOCK_HEADER: {
		if (*in_pos >= in_size)
			return LZMA_OK;

		if (coder->buffer_pos == 0) {
			// Check for an unexpected Index Indicator.
			if (in[*in_pos] == INDEX_INDICATOR) {
				// Consume the Index Indicator. cur_in_offset
				// doesn't matter because one cannot recover
				// from LZMA_DATA_ERROR by seeking.
				++*in_pos;
				return LZMA_DATA_ERROR;
			}

			// Calculate the size of the Block Header. Note that
			// Block Header decoder wants to see this byte too
			// so don't advance *in_pos.
			coder->block_options.header_size
					= lzma_block_header_size_decode(
						in[*in_pos]);
		}

		// Copy the Block Header to the internal buffer.
		const size_t in_start = *in_pos;
		lzma_bufcpy(in, in_pos, in_size,
				coder->buffer, &coder->buffer_pos,
				coder->block_options.header_size);
		coder->cur_in_offset += *in_pos - in_start;

		// Return if we didn't get the whole Block Header yet.
		if (coder->buffer_pos < coder->block_options.header_size)
			return LZMA_OK;

		coder->sequence = SEQ_BLOCK_INIT;
		FALLTHROUGH;
	}

	case SEQ_BLOCK_INIT: {
		// Checking memusage and doing the initialization needs
		// its own sequence point because we need to be able to
		// retry if we return LZMA_MEMLIMIT_ERROR.

		// Version 1 is needed to support the .ignore_check option.
		coder->block_options.version = 1;

		// Copy the Check type from Stream Header.
		coder->block_options.check = coder->iter.stream.flags->check;

		// Set up a buffer to hold the filter chain. Block Header
		// decoder will initialize all members of this array so
		// we don't need to do it here.
		lzma_filter filters[LZMA_FILTERS_MAX + 1];
		coder->block_options.filters = filters;

		// Decode the Block Header.
		return_if_error(lzma_block_header_decode(&coder->block_options,
				allocator, coder->buffer));

		// The filters array has been allocated. Track the errors
		// with this variable and free the filters after initializing
		// the Block decoder or if an error occurred.
		lzma_ret ret = LZMA_OK;

		// If Compressed Size and/or Uncompressed Size are present,
		// verify that they match the Index.
		if (coder->block_options.compressed_size != LZMA_VLI_UNKNOWN
				&& coder->block_options.compressed_size
				!= coder->iter.block.unpadded_size
				- lzma_check_size(coder->block_options.check)
				- coder->block_options.header_size)
			ret = LZMA_DATA_ERROR;

		if (coder->block_options.uncompressed_size != LZMA_VLI_UNKNOWN
				&& coder->block_options.uncompressed_size
					!= coder->iter.block.uncompressed_size)
			ret = LZMA_DATA_ERROR;

		// Check the memory usage limit.
		if (ret == LZMA_OK) {
			const uint64_t memusage
					= lzma_raw_decoder_memusage(filters);
			if (memusage == UINT64_MAX) {
				// One or more unknown Filter IDs.
				ret = LZMA_OPTIONS_ERROR;
			} else {
				// The filter chain is valid. We don't want
				// lzma_memusage() to return UINT64_MAX in
				// case of invalid filter chain, so we
				// set coder->memusage only now.
				coder->memusage = memusage;

				if (memusage > coder->memlimit) {
					// The chain would use too much memory.
					ret = LZMA_MEMLIMIT_ERROR;
				}
			}
		}

		if (ret == LZMA_OK) {
			// The sizes (if present) in Block Header match
			// the Index and the memory usage is low enough.
			//
			// Set Compressed Size and Uncompressed Size from
			// Index so that Block decoder will verify them.
			coder->block_options.compressed_size
				= coder->iter.block.unpadded_size
				- lzma_check_size(coder->block_options.check)
				- coder->block_options.header_size;

			coder->block_options.uncompressed_size
					= coder->iter.block.uncompressed_size;

			// If LZMA_IGNORE_CHECK was used, this flag needs
			// to be set. It has to be set after
			// lzma_block_header_decode() because
			// it always resets this to false.
			coder->block_options.ignore_check
					= coder->ignore_check;

			// Initialize the Block decoder.
			ret = lzma_block_decoder_init(&coder->next, allocator,
					&coder->block_options);
		}

		// Free the allocated filter options since they are needed
		// only to initialize the Block decoder.
		lzma_filters_free(filters, allocator);
		coder->block_options.filters = NULL;

		// Check if Block decoder was initialized.
		if (ret != LZMA_OK)
			return ret;

		coder->sequence = SEQ_BLOCK_SKIP;
		FALLTHROUGH;
	}

	case SEQ_BLOCK_SKIP: {
		// Decode and and throw away output until we reach
		// the requested uncompressed offset.
		while (coder->skip_out > 0) {
			size_t skip_out_pos = 0;
			const size_t skip_out_size = my_min(
					sizeof(coder->buffer),
					coder->skip_out);

			const size_t in_start = *in_pos;
			const lzma_ret ret = coder->next.code(
					coder->next.coder, allocator,
					in, in_pos, in_size,
					coder->buffer, &skip_out_pos,
					skip_out_size, LZMA_RUN);

			coder->skip_out -= skip_out_pos;
			coder->cur_in_offset += *in_pos - in_start;
			coder->cur_out_offset += skip_out_pos;

			if (ret != LZMA_OK) {
				// If we reach the end of the Block, it means
				// that coder->index doesn't match the .xz
				// file contents.
				return ret == LZMA_STREAM_END
						? LZMA_DATA_ERROR : ret;
			}

			if (skip_out_pos < skip_out_size) {
				// We need more input to produce more output.
				assert(*in_pos == in_size);
				return LZMA_OK;
			}
		}

		coder->sequence = SEQ_BLOCK_OUTPUT;
		FALLTHROUGH;
	}

	case SEQ_BLOCK_OUTPUT: {
		// Decode and pass the data to the application.
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(
				coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				LZMA_RUN);

		coder->cur_in_offset += *in_pos - in_start;
		coder->cur_out_offset += *out_pos - out_start;

		if (ret != LZMA_STREAM_END)
			return ret;

		// Block decoded successfully. Because we told the expected
		// values of Compressed Size and Uncompressed Size to the
		// Block decoder, it has verified that they match the Index;
		// we don't need to check them here.
		coder->sequence = SEQ_ITER_NEXT_BLOCK;
		break;
	}

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

	// Never reached
}


static void
seekable_decoder_end(void *coder_ptr, const lzma_allocator *allocator)
{
	lzma_seekable_coder *coder = coder_ptr;
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
seekable_decoder_memconfig(void *coder_ptr, uint64_t *memusage,
		uint64_t *old_memlimit, uint64_t new_memlimit)
{
	lzma_seekable_coder *coder = coder_ptr;

	*memusage = coder->memusage;
	*old_memlimit = coder->memlimit;

	if (new_memlimit != 0) {
		if (new_memlimit < coder->memusage)
			return LZMA_MEMLIMIT_ERROR;

		coder->memlimit = new_memlimit;
	}

	return LZMA_OK;
}


static lzma_ret
seekable_decoder_init(
		lzma_next_coder *next, const lzma_allocator *allocator,
		uint64_t memlimit, uint32_t flags,
		const lzma_index *idx, uint64_t *seek_pos)
{
	lzma_next_coder_init(&seekable_decoder_init, next, allocator);

	assert(seek_pos != NULL);

	if (idx == NULL)
		return LZMA_PROG_ERROR;

	// Not many flags are supported.
	if (flags & ~LZMA_IGNORE_CHECK)
		return LZMA_OPTIONS_ERROR;

	lzma_seekable_coder *coder = next->coder;
	if (coder == NULL) {
		coder = lzma_alloc(sizeof(lzma_seekable_coder), allocator);
		if (coder == NULL)
			return LZMA_MEM_ERROR;

		next->coder = coder;
		next->code = &seekable_decode;
		next->end = &seekable_decoder_end;
		next->memconfig = &seekable_decoder_memconfig;

		coder->next = LZMA_NEXT_CODER_INIT;
	}

	coder->sequence = SEQ_ITER_NEXT_BLOCK;

	coder->memlimit = my_max(1, memlimit);
	coder->memusage = LZMA_MEMUSAGE_BASE;

	coder->ignore_check = (flags & LZMA_IGNORE_CHECK) != 0;

	lzma_index_iter_init(&coder->iter, idx);
	coder->external_seek_pos = seek_pos;

	coder->cur_in_offset = 0;
	coder->cur_out_offset = 0;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_seekable_decoder(lzma_stream *strm, uint64_t memlimit, uint32_t flags,
		const lzma_index *idx)
{
	lzma_next_strm_init(seekable_decoder_init, strm, memlimit, flags,
			idx, &strm->seek_pos);

	// We allow LZMA_FINISH in addition to LZMA_RUN in case it's
	// convenient in some application. lzma_code() is able to handle
	// the LZMA_FINISH + LZMA_SEEK_NEEDED and also allows changing
	// from LZMA_FINISH to LZMA_SEEK_TO_OFFSET or LZMA_SEEK_TO_BLOCK.
	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;
	strm->internal->supported_actions[LZMA_SEEK_TO_OFFSET] = true;
	strm->internal->supported_actions[LZMA_SEEK_TO_BLOCK] = true;

	return LZMA_OK;
}
