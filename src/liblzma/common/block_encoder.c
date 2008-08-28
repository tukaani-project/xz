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
#include "filter_encoder.h"
#include "check.h"


struct lzma_coder_s {
	/// The filters in the chain; initialized with lzma_raw_decoder_init().
	lzma_next_coder next;

	/// Encoding options; we also write Total Size, Compressed Size, and
	/// Uncompressed Size back to this structure when the encoding has
	/// been finished.
	lzma_block *options;

	enum {
		SEQ_CODE,
		SEQ_PADDING,
		SEQ_CHECK,
	} sequence;

	/// Compressed Size calculated while encoding
	lzma_vli compressed_size;

	/// Uncompressed Size calculated while encoding
	lzma_vli uncompressed_size;

	/// Position when writing out the Check field
	size_t check_pos;

	/// Check of the uncompressed data
	lzma_check_state check;
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
				return LZMA_PROG_ERROR;
		} else {
			if (coder->options->uncompressed_size
					- coder->uncompressed_size
					<  (lzma_vli)(in_size - *in_pos))
				return LZMA_PROG_ERROR;
		}
	} else if (LZMA_VLI_VALUE_MAX - coder->uncompressed_size
			< (lzma_vli)(in_size - *in_pos)) {
		return LZMA_PROG_ERROR;
	}

	// Main loop
	while (*out_pos < out_size
			&& (*in_pos < in_size || action != LZMA_RUN))
	switch (coder->sequence) {
	case SEQ_CODE: {
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

		const size_t in_used = *in_pos - in_start;
		const size_t out_used = *out_pos - out_start;

		// FIXME We must also check that Total Size doesn't get
		// too big.
		if (update_size(&coder->compressed_size, out_used,
				coder->options->compressed_size))
			return LZMA_DATA_ERROR;

		// No need to check for overflow because we have already
		// checked it at the beginning of this function.
		coder->uncompressed_size += in_used;

		lzma_check_update(&coder->check, coder->options->check,
				in + in_start, in_used);

		if (ret != LZMA_STREAM_END || action == LZMA_SYNC_FLUSH)
			return ret;

		assert(*in_pos == in_size);
		coder->sequence = SEQ_PADDING;
		break;
	}

	case SEQ_PADDING:
		// Pad Compressed Data to a multiple of four bytes.
		if (coder->compressed_size & 3) {
			out[*out_pos] = 0x00;
			++*out_pos;

			if (update_size(&coder->compressed_size, 1,
					coder->options->compressed_size))
				return LZMA_DATA_ERROR;

			break;
		}

		// Compressed and Uncompressed Sizes are now at their final
		// values. Verify that they match the values given to us.
		if (!is_size_valid(coder->compressed_size,
					coder->options->compressed_size)
				|| !is_size_valid(coder->uncompressed_size,
					coder->options->uncompressed_size))
			return LZMA_DATA_ERROR;

		// Copy the values into coder->options. The caller
		// may use this information to construct Index.
		coder->options->compressed_size = coder->compressed_size;
		coder->options->uncompressed_size = coder->uncompressed_size;

		if (coder->options->check == LZMA_CHECK_NONE)
			return LZMA_STREAM_END;

		lzma_check_finish(&coder->check, coder->options->check);
		coder->sequence = SEQ_CHECK;

	// Fall through

	case SEQ_CHECK:
		out[*out_pos] = coder->check.buffer.u8[coder->check_pos];
		++*out_pos;

		if (++coder->check_pos
				== lzma_check_size(coder->options->check))
			return LZMA_STREAM_END;

		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
block_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_block_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_block *options)
{
	lzma_next_coder_init(lzma_block_encoder_init, next, allocator);

	// While lzma_block_total_size_get() is meant to calculate the Total
	// Size, it also validates the options excluding the filters.
	if (lzma_block_total_size_get(options) == 0)
		return LZMA_PROG_ERROR;

	// If the Check ID is not supported, we cannot calculate the check and
	// thus not create a proper Block.
	if ((unsigned)(options->check) > LZMA_CHECK_ID_MAX)
		return LZMA_PROG_ERROR;

	if (!lzma_check_is_supported(options->check))
		return LZMA_UNSUPPORTED_CHECK;

	// Allocate and initialize *next->coder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_encode;
		next->end = &block_encoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Basic initializations
	next->coder->sequence = SEQ_CODE;
	next->coder->options = options;
	next->coder->compressed_size = 0;
	next->coder->uncompressed_size = 0;

	// Initialize the check
	next->coder->check_pos = 0;
	lzma_check_init(&next->coder->check, options->check);

	// Initialize the requested filters.
	return lzma_raw_encoder_init(&next->coder->next, allocator,
			options->filters);
}


extern LZMA_API lzma_ret
lzma_block_encoder(lzma_stream *strm, lzma_block *options)
{
	lzma_next_strm_init(lzma_block_encoder_init, strm, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
