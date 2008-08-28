///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_decoder.c
/// \brief      Decodes .lzma Blocks
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

#include "block_decoder.h"
#include "block_private.h"
#include "filter_decoder.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_CODE,
		SEQ_PADDING,
		SEQ_CHECK,
	} sequence;

	/// The filters in the chain; initialized with lzma_raw_decoder_init().
	lzma_next_coder next;

	/// Decoding options; we also write Compressed Size and Uncompressed
	/// Size back to this structure when the encoding has been finished.
	lzma_block *options;

	/// Compressed Size calculated while encoding
	lzma_vli compressed_size;

	/// Uncompressed Size calculated while encoding
	lzma_vli uncompressed_size;

	/// Maximum allowed Compressed Size; this takes into account the
	/// size of the Block Header and Check fields when Compressed Size
	/// is unknown.
	lzma_vli compressed_limit;

	/// Position when reading the Check field
	size_t check_pos;

	/// Check of the uncompressed data
	lzma_check_state check;
};


static lzma_ret
block_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	switch (coder->sequence) {
	case SEQ_CODE: {
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

		const size_t in_used = *in_pos - in_start;
		const size_t out_used = *out_pos - out_start;

		// NOTE: We compare to compressed_limit here, which prevents
		// the total size of the Block growing past LZMA_VLI_VALUE_MAX.
		if (update_size(&coder->compressed_size, in_used,
					coder->compressed_limit)
				|| update_size(&coder->uncompressed_size,
					out_used,
					coder->options->uncompressed_size))
			return LZMA_DATA_ERROR;

		lzma_check_update(&coder->check, coder->options->check,
				out + out_start, out_used);

		if (ret != LZMA_STREAM_END)
			return ret;

		coder->sequence = SEQ_PADDING;
	}

	// Fall through

	case SEQ_PADDING:
		// Compressed Data is padded to a multiple of four bytes.
		while (coder->compressed_size & 3) {
			if (*in_pos >= in_size)
				return LZMA_OK;

			if (in[(*in_pos)++] != 0x00)
				return LZMA_DATA_ERROR;

			if (update_size(&coder->compressed_size, 1,
					coder->compressed_limit))
				return LZMA_DATA_ERROR;
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

	case SEQ_CHECK: {
		const bool chksup = lzma_check_is_supported(
				coder->options->check);

		while (*in_pos < in_size) {
			// coder->check.buffer[] may be uninitialized when
			// the Check ID is not supported.
			if (chksup && coder->check.buffer.u8[coder->check_pos]
					!= in[*in_pos]) {
				++*in_pos;
				return LZMA_DATA_ERROR;
			}

			++*in_pos;

			if (++coder->check_pos == lzma_check_size(
					coder->options->check))
				return LZMA_STREAM_END;
		}

		return LZMA_OK;
	}
	}

	return LZMA_PROG_ERROR;
}


static void
block_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_block_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_block *options)
{
	lzma_next_coder_init(lzma_block_decoder_init, next, allocator);

	// While lzma_block_total_size_get() is meant to calculate the Total
	// Size, it also validates the options excluding the filters.
	if (lzma_block_total_size_get(options) == 0)
		return LZMA_PROG_ERROR;

	// options->check is used for array indexing so we need to know that
	// it is in the valid range.
	if ((unsigned)(options->check) > LZMA_CHECK_ID_MAX)
		return LZMA_PROG_ERROR;

	// Allocate and initialize *next->coder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_decode;
		next->end = &block_decoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Basic initializations
	next->coder->sequence = SEQ_CODE;
	next->coder->options = options;
	next->coder->compressed_size = 0;
	next->coder->uncompressed_size = 0;

	// If Compressed Size is not known, we calculate the maximum allowed
	// value so that Total Size of the Block still is a valid VLI and
	// a multiple of four.
	next->coder->compressed_limit
			= options->compressed_size == LZMA_VLI_VALUE_UNKNOWN
				? (LZMA_VLI_VALUE_MAX & ~LZMA_VLI_C(3))
					- options->header_size
					- lzma_check_size(options->check)
				: options->compressed_size;

	// Initialize the check. It's caller's problem if the Check ID is not
	// supported, and the Block decoder cannot verify the Check field.
	// Caller can test lzma_checks[options->check].
	next->coder->check_pos = 0;
	lzma_check_init(&next->coder->check, options->check);

	// Initialize the filter chain.
	return lzma_raw_decoder_init(&next->coder->next, allocator,
			options->filters);
}


extern LZMA_API lzma_ret
lzma_block_decoder(lzma_stream *strm, lzma_block *options)
{
	lzma_next_strm_init(lzma_block_decoder_init, strm, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
