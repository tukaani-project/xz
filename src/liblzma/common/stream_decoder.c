///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_decoder.c
/// \brief      Decodes .lzma Streams
//
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "stream_common.h"
#include "stream_decoder.h"
#include "check.h"
#include "stream_flags_decoder.h"
#include "block_decoder.h"


struct lzma_coder_s {
	enum {
		SEQ_STREAM_HEADER,
		SEQ_BLOCK_HEADER,
		SEQ_BLOCK,
		SEQ_INDEX,
		SEQ_STREAM_FOOTER,
	} sequence;

	/// Block or Metadata decoder. This takes little memory and the same
	/// data structure can be used to decode every Block Header, so it's
	/// a good idea to have a separate lzma_next_coder structure for it.
	lzma_next_coder block_decoder;

	/// Block options decoded by the Block Header decoder and used by
	/// the Block decoder.
	lzma_options_block block_options;

	/// Stream Flags from Stream Header
	lzma_stream_flags stream_flags;

	/// Index is hashed so that it can be compared to the sizes of Blocks
	/// with O(1) memory usage.
	lzma_index_hash *index_hash;

	/// Write position in buffer[]
	size_t buffer_pos;

	/// Buffer to hold Stream Header, Block Header, and Stream Footer.
	/// Block Header has biggest maximum size.
	uint8_t buffer[LZMA_BLOCK_HEADER_SIZE_MAX];
};


static lzma_ret
stream_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// When decoding the actual Block, it may be able to produce more
	// output even if we don't give it any new input.
	while (*out_pos < out_size && (*in_pos < in_size
			|| coder->sequence == SEQ_BLOCK))
	switch (coder->sequence) {
	case SEQ_STREAM_HEADER: {
		// Copy the Stream Header to the internal buffer.
		bufcpy(in, in_pos, in_size, coder->buffer, &coder->buffer_pos,
				LZMA_STREAM_HEADER_SIZE);

		// Return if we didn't get the whole Stream Header yet.
		if (coder->buffer_pos < LZMA_STREAM_HEADER_SIZE)
			return LZMA_OK;

		coder->buffer_pos = 0;

		// Decode the Stream Header.
		return_if_error(lzma_stream_header_decode(
				&coder->stream_flags, coder->buffer));

		// Copy the type of the Check so that Block Header and Block
		// decoders see it.
		coder->block_options.check = coder->stream_flags.check;

		// Even if we return LZMA_UNSUPPORTED_CHECK below, we want
		// to continue from Block Header decoding.
		coder->sequence = SEQ_BLOCK_HEADER;

		// Detect if the Check type is supported and give appropriate
		// warning if it isn't. We don't warn every time a new Block
		// is started.
		if (!lzma_available_checks[coder->block_options.check])
			return LZMA_UNSUPPORTED_CHECK;

		break;
	}

	case SEQ_BLOCK_HEADER: {
		if (coder->buffer_pos == 0) {
			// Detect if it's Index.
			if (in[*in_pos] == 0x00) {
				coder->sequence = SEQ_INDEX;
				break;
			}

			// Calculate the size of the Block Header. Note that
			// Block Header decoder wants to see this byte too
			// so don't advance *in_pos.
			coder->block_options.header_size
					= lzma_block_header_size_decode(
						in[*in_pos]);
		}

		// Copy the Block Header to the internal buffer.
		bufcpy(in, in_pos, in_size, coder->buffer, &coder->buffer_pos,
				coder->block_options.header_size);

		// Return if we didn't get the whole Block Header yet.
		if (coder->buffer_pos < coder->block_options.header_size)
			return LZMA_OK;

		coder->buffer_pos = 0;

		// Set up a buffer to hold the filter chain. Block Header
		// decoder will initialize all members of this array so
		// we don't need to do it here.
		lzma_options_filter filters[LZMA_BLOCK_FILTERS_MAX + 1];
		coder->block_options.filters = filters;

		// Decode the Block Header.
		return_if_error(lzma_block_header_decode(&coder->block_options,
				allocator, coder->buffer));

		// Initialize the Block decoder.
		const lzma_ret ret = lzma_block_decoder_init(
				&coder->block_decoder,
				allocator, &coder->block_options);

		// Free the allocated filter options since they are needed
		// only to initialize the Block decoder.
		for (size_t i = 0; i < LZMA_BLOCK_FILTERS_MAX; ++i)
			lzma_free(filters[i].options, allocator);

		coder->block_options.filters = NULL;

		// Check if Block enocoder initialization succeeded. Don't
		// warn about unsupported check anymore since we did it
		// earlier if it was needed.
		if (ret != LZMA_OK && ret != LZMA_UNSUPPORTED_CHECK)
			return ret;

		coder->sequence = SEQ_BLOCK;
		break;
	}

	case SEQ_BLOCK: {
		lzma_ret ret = coder->block_decoder.code(
				coder->block_decoder.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

		if (ret != LZMA_STREAM_END)
			return ret;

		// Block decoded successfully. Add the new size pair to
		// the Index hash.
		return_if_error(lzma_index_hash_append(coder->index_hash,
				lzma_block_total_size_get(
					&coder->block_options),
				coder->block_options.uncompressed_size));

		coder->sequence = SEQ_BLOCK_HEADER;
		break;
	}

	case SEQ_INDEX: {
		// Decode the Index and compare it to the hash calculated
		// from the sizes of the Blocks (if any).
		const lzma_ret ret = lzma_index_hash_decode(coder->index_hash,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			return ret;

		coder->sequence = SEQ_STREAM_FOOTER;
		break;
	}

	case SEQ_STREAM_FOOTER:
		// Copy the Stream Footer to the internal buffer.
		bufcpy(in, in_pos, in_size, coder->buffer, &coder->buffer_pos,
				LZMA_STREAM_HEADER_SIZE);

		// Return if we didn't get the whole Stream Footer yet.
		if (coder->buffer_pos < LZMA_STREAM_HEADER_SIZE)
			return LZMA_OK;

		// Decode the Stream Footer.
		lzma_stream_flags footer_flags;
		return_if_error(lzma_stream_footer_decode(
				&footer_flags, coder->buffer));

		// Check that Index Size stored in the Stream Footer matches
		// the real size of the Index field.
		if (lzma_index_hash_size(coder->index_hash)
				!= footer_flags.backward_size)
			return LZMA_DATA_ERROR;

		// Compare that the Stream Flags fields are identical in
		// both Stream Header and Stream Footer.
		if (!lzma_stream_flags_equal(&coder->stream_flags,
				&footer_flags))
			return LZMA_DATA_ERROR;

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
stream_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->block_decoder, allocator);
	lzma_index_hash_end(coder->index_hash, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
stream_decoder_init(lzma_next_coder *next, lzma_allocator *allocator)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &stream_decode;
		next->end = &stream_decoder_end;

		next->coder->block_decoder = LZMA_NEXT_CODER_INIT;
		next->coder->index_hash = NULL;
	}

	// Initialize the Index hash used to verify the Index.
	next->coder->index_hash = lzma_index_hash_init(
			next->coder->index_hash, allocator);
	if (next->coder->index_hash == NULL)
		return LZMA_MEM_ERROR;

	// Reset the rest of the variables.
	next->coder->sequence = SEQ_STREAM_HEADER;
	next->coder->block_options.filters = NULL;
	next->coder->buffer_pos = 0;

	return LZMA_OK;
}


extern lzma_ret
lzma_stream_decoder_init(lzma_next_coder *next, lzma_allocator *allocator)
{
	lzma_next_coder_init0(stream_decoder_init, next, allocator);
}


extern LZMA_API lzma_ret
lzma_stream_decoder(lzma_stream *strm)
{
	lzma_next_strm_init0(strm, stream_decoder_init);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
