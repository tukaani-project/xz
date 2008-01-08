///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_encoder.c
/// \brief      Encodes .lzma Blocks
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

#include "block_encoder.h"
#include "block_private.h"
#include "raw_encoder.h"
#include "check.h"


struct lzma_coder_s {
	/// The filters in the chain; initialized with lzma_raw_decoder_init().
	lzma_next_coder next;

	/// Encoding options; we also write Total Size, Compressed Size, and
	/// Uncompressed Size back to this structure when the encoding has
	/// been finished.
	lzma_options_block *options;

	enum {
		SEQ_CODE,
		SEQ_CHECK_FINISH,
		SEQ_CHECK_COPY,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_BACKWARD_SIZE,
		SEQ_PADDING,
	} sequence;

	/// Position in .header and .check.
	size_t pos;

	/// Check of the uncompressed data
	lzma_check check;

	/// Total Size calculated while encoding
	lzma_vli total_size;

	/// Compressed Size calculated while encoding
	lzma_vli compressed_size;

	/// Uncompressed Size calculated while encoding
	lzma_vli uncompressed_size;

	/// Maximum allowed total_size
	lzma_vli total_limit;

	/// Maximum allowed uncompressed_size
	lzma_vli uncompressed_limit;

	/// Backward Size - This is a copy of total_size right before
	/// the Backward Size field.
	lzma_vli backward_size;
};


static lzma_ret
block_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Check that our amount of input stays in proper limits.
	if (coder->options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (action == LZMA_FINISH) {
			if (coder->options->uncompressed_size
					- coder->uncompressed_size
					!= (lzma_vli)(in_size - *in_pos))
				return LZMA_DATA_ERROR;
		} else {
			if (coder->options->uncompressed_size
					- coder->uncompressed_size
					<  (lzma_vli)(in_size - *in_pos))
				return LZMA_DATA_ERROR;
		}
	} else if (LZMA_VLI_VALUE_MAX - coder->uncompressed_size
			< (lzma_vli)(in_size - *in_pos)) {
		return LZMA_DATA_ERROR;
	}

	// Main loop
	while (*out_pos < out_size
			&& (*in_pos < in_size || action == LZMA_FINISH))
	switch (coder->sequence) {
	case SEQ_CODE: {
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

		const size_t in_used = *in_pos - in_start;
		const size_t out_used = *out_pos - out_start;

		if (update_size(&coder->total_size, out_used,
					coder->total_limit)
				|| update_size(&coder->compressed_size,
					out_used,
					coder->options->compressed_size))
			return LZMA_DATA_ERROR;

		// No need to check for overflow because we have already
		// checked it at the beginning of this function.
		coder->uncompressed_size += in_used;

		lzma_check_update(&coder->check, coder->options->check,
				in + in_start, in_used);

		if (ret != LZMA_STREAM_END)
			return ret;

		assert(*in_pos == in_size);

		// Compressed and Uncompressed Sizes are now at their final
		// values. Verify that they match the values give to us.
		if (!is_size_valid(coder->compressed_size,
					coder->options->compressed_size)
				|| !is_size_valid(coder->uncompressed_size,
					coder->options->uncompressed_size))
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_CHECK_FINISH;
		break;
	}

	case SEQ_CHECK_FINISH:
		if (coder->options->check == LZMA_CHECK_NONE) {
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
			break;
		}

		lzma_check_finish(&coder->check, coder->options->check);
		coder->sequence = SEQ_CHECK_COPY;

	// Fall through

	case SEQ_CHECK_COPY:
		assert(lzma_check_sizes[coder->options->check] > 0);

		switch (coder->options->check) {
		case LZMA_CHECK_CRC32:
			out[*out_pos] = coder->check.crc32 >> (coder->pos * 8);
			break;

		case LZMA_CHECK_CRC64:
			out[*out_pos] = coder->check.crc64 >> (coder->pos * 8);
			break;

		case LZMA_CHECK_SHA256:
			out[*out_pos] = coder->check.sha256.buffer[coder->pos];
			break;

		default:
			assert(0);
			return LZMA_PROG_ERROR;
		}

		++*out_pos;

		if (update_size(&coder->total_size, 1, coder->total_limit))
			return LZMA_DATA_ERROR;

		if (++coder->pos == lzma_check_sizes[coder->options->check]) {
			coder->pos = 0;
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		}

		break;

	case SEQ_UNCOMPRESSED_SIZE:
		if (coder->options->has_uncompressed_size_in_footer) {
			const size_t out_start = *out_pos;

			const lzma_ret ret = lzma_vli_encode(
					coder->uncompressed_size,
					&coder->pos, 1,
					out, out_pos, out_size);

			// Updating the size this way instead of doing in a
			// single chunk using lzma_vli_size(), because this
			// way we detect when exactly we are going out of
			// our limits.
			if (update_size(&coder->total_size,
					*out_pos - out_start,
					coder->total_limit))
				return LZMA_DATA_ERROR;

			if (ret != LZMA_STREAM_END)
				return ret;

			coder->pos = 0;
		}

		coder->backward_size = coder->total_size;
		coder->sequence = SEQ_BACKWARD_SIZE;
		break;

	case SEQ_BACKWARD_SIZE:
		if (coder->options->has_backward_size) {
			const size_t out_start = *out_pos;

			const lzma_ret ret = lzma_vli_encode(
					coder->backward_size, &coder->pos, 1,
					out, out_pos, out_size);

			if (update_size(&coder->total_size,
					*out_pos - out_start,
					coder->total_limit))
				return LZMA_DATA_ERROR;

			if (ret != LZMA_STREAM_END)
				return ret;
		}

		coder->sequence = SEQ_PADDING;
		break;

	case SEQ_PADDING:
		if (coder->options->handle_padding) {
			assert(!coder->options
					->has_uncompressed_size_in_footer);
			assert(!coder->options->has_backward_size);
			assert(coder->options->total_size != LZMA_VLI_VALUE_UNKNOWN);

			if (coder->total_size < coder->options->total_size) {
				out[*out_pos] = 0x00;
				++*out_pos;

				if (update_size(&coder->total_size, 1,
						coder->total_limit))
					return LZMA_DATA_ERROR;

				break;
			}
		}

		// Now also Total Size is known. Verify it.
		if (!is_size_valid(coder->total_size,
					coder->options->total_size))
			return LZMA_DATA_ERROR;

		// Copy the values into coder->options. The caller
		// may use this information to construct Index.
		coder->options->total_size = coder->total_size;
		coder->options->compressed_size = coder->compressed_size;
		coder->options->uncompressed_size = coder->uncompressed_size;

		return LZMA_STREAM_END;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
block_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
block_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options)
{
	// Validate some options.
	if (options == NULL
			|| !lzma_vli_is_valid(options->total_size)
			|| !lzma_vli_is_valid(options->compressed_size)
			|| !lzma_vli_is_valid(options->uncompressed_size)
			|| !lzma_vli_is_valid(options->total_size)
			|| !lzma_vli_is_valid(options->total_limit)
			|| !lzma_vli_is_valid(options->uncompressed_limit)
			|| (options->uncompressed_size
					!= LZMA_VLI_VALUE_UNKNOWN
				&& options->uncompressed_size
					> options->uncompressed_limit)
			|| (options->total_size != LZMA_VLI_VALUE_UNKNOWN
				&& options->total_size
					> options->total_limit)
			|| (!options->has_eopm && options->uncompressed_size
				== LZMA_VLI_VALUE_UNKNOWN)
			|| (options->handle_padding && (options->total_size
					== LZMA_VLI_VALUE_UNKNOWN
				|| options->has_uncompressed_size_in_footer
				|| options->has_backward_size))
			|| options->header_size > options->total_size)
		return LZMA_PROG_ERROR;

	// Allocate and initialize *next->coder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_encode;
		next->end = &block_encoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Initialize the check.
	return_if_error(lzma_check_init(&next->coder->check, options->check));

	// If End of Payload Marker is not used and Uncompressed Size is zero,
	// Compressed Data is empty. That is, we don't call the encoder at all.
	// We initialize it though; it allows detecting invalid options.
	if (!options->has_eopm && options->uncompressed_size == 0) {
		// Also Compressed Size must also be zero if it has been
		// given to us.
		if (!is_size_valid(0, options->compressed_size))
			return LZMA_PROG_ERROR;

		next->coder->sequence = SEQ_CHECK_FINISH;
	} else {
		next->coder->sequence = SEQ_CODE;
	}

	// Other initializations
	next->coder->options = options;
	next->coder->pos = 0;
	next->coder->total_size = options->header_size;
	next->coder->compressed_size = 0;
	next->coder->uncompressed_size = 0;
	next->coder->total_limit
			= MIN(options->total_size, options->total_limit);
	next->coder->uncompressed_limit = MIN(options->uncompressed_size,
			options->uncompressed_limit);

	// Initialize the requested filters.
	return lzma_raw_encoder_init(&next->coder->next, allocator,
			options->filters, options->has_eopm
				? LZMA_VLI_VALUE_UNKNOWN
				: options->uncompressed_size,
			true);
}


extern lzma_ret
lzma_block_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options)
{
	lzma_next_coder_init(block_encoder_init, next, allocator, options);
}


extern LZMA_API lzma_ret
lzma_block_encoder(lzma_stream *strm, lzma_options_block *options)
{
	lzma_next_strm_init(strm, block_encoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
