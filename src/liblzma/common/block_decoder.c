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
#include "raw_decoder.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_CODE,
		SEQ_CHECK,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_BACKWARD_SIZE,
		SEQ_PADDING,
		SEQ_END,
	} sequence;

	/// The filters in the chain; initialized with lzma_raw_decoder_init().
	lzma_next_coder next;

	/// Decoding options; we also write Total Size, Compressed Size, and
	/// Uncompressed Size back to this structure when the encoding has
	/// been finished.
	lzma_options_block *options;

	/// Position in variable-length integers (and in some other places).
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

	/// Temporary location for the Uncompressed Size and Backward Size
	/// fields in Block Footer.
	lzma_vli tmp;

	/// Size of the Backward Size field - This is needed so that we
	/// can verify the Backward Size and still keep updating total_size.
	size_t size_of_backward_size;
};


static lzma_ret
update_sequence(lzma_coder *coder)
{
	switch (coder->sequence) {
	case SEQ_CODE:
		if (coder->options->check != LZMA_CHECK_NONE) {
			lzma_check_finish(&coder->check,
					coder->options->check);
			coder->sequence = SEQ_CHECK;
			break;
		}

	// Fall through

	case SEQ_CHECK:
		if (coder->options->has_uncompressed_size_in_footer) {
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
			break;
		}

	// Fall through

	case SEQ_UNCOMPRESSED_SIZE:
		if (coder->options->has_backward_size) {
			coder->sequence = SEQ_BACKWARD_SIZE;
			break;
		}

	// Fall through

	case SEQ_BACKWARD_SIZE:
		if (coder->options->handle_padding) {
			coder->sequence = SEQ_PADDING;
			break;
		}

	case SEQ_PADDING:
		if (!is_size_valid(coder->total_size,
					coder->options->total_size)
				|| !is_size_valid(coder->compressed_size,
					coder->options->compressed_size)
				|| !is_size_valid(coder->uncompressed_size,
					coder->options->uncompressed_size))
			return LZMA_DATA_ERROR;

		// Copy the values into coder->options. The caller
		// may use this information to construct Index.
		coder->options->total_size = coder->total_size;
		coder->options->compressed_size = coder->compressed_size;
		coder->options->uncompressed_size = coder->uncompressed_size;

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
block_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Special case when the Block has only Block Header.
	if (coder->sequence == SEQ_END)
		return LZMA_STREAM_END;

	// FIXME: Termination condition should work but could be cleaner.
	while (*out_pos < out_size && (*in_pos < in_size
			|| coder->sequence == SEQ_CODE))
	switch (coder->sequence) {
	case SEQ_CODE: {
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

		const size_t in_used = *in_pos - in_start;
		const size_t out_used = *out_pos - out_start;

		if (update_size(&coder->total_size, in_used,
					coder->total_limit)
				|| update_size(&coder->compressed_size,
					in_used,
					coder->options->compressed_size)
				|| update_size(&coder->uncompressed_size,
					out_used, coder->uncompressed_limit))
			return LZMA_DATA_ERROR;

		lzma_check_update(&coder->check, coder->options->check,
				out + out_start, out_used);

		if (ret != LZMA_STREAM_END)
			return ret;

		return_if_error(update_sequence(coder));

		break;
	}

	case SEQ_CHECK:
		switch (coder->options->check) {
		case LZMA_CHECK_CRC32:
			if (((coder->check.crc32 >> (coder->pos * 8))
					& 0xFF) != in[*in_pos])
				return LZMA_DATA_ERROR;
			break;

		case LZMA_CHECK_CRC64:
			if (((coder->check.crc64 >> (coder->pos * 8))
					& 0xFF) != in[*in_pos])
				return LZMA_DATA_ERROR;
			break;

		case LZMA_CHECK_SHA256:
			if (coder->check.sha256.buffer[coder->pos]
					!= in[*in_pos])
				return LZMA_DATA_ERROR;
			break;

		default:
			assert(coder->options->check != LZMA_CHECK_NONE);
			assert(coder->options->check <= LZMA_CHECK_ID_MAX);
			break;
		}

		if (update_size(&coder->total_size, 1, coder->total_limit))
			return LZMA_DATA_ERROR;

		++*in_pos;

		if (++coder->pos == lzma_check_sizes[coder->options->check]) {
			return_if_error(update_sequence(coder));
			coder->pos = 0;
		}

		break;

	case SEQ_UNCOMPRESSED_SIZE: {
		const size_t in_start = *in_pos;

		const lzma_ret ret = lzma_vli_decode(&coder->tmp,
				&coder->pos, in, in_pos, in_size);

		if (update_size(&coder->total_size, *in_pos - in_start,
				coder->total_limit))
			return LZMA_DATA_ERROR;

		if (ret != LZMA_STREAM_END)
			return ret;

		if (coder->tmp != coder->uncompressed_size)
			return LZMA_DATA_ERROR;

		coder->pos = 0;
		coder->tmp = 0;

		return_if_error(update_sequence(coder));

		break;
	}

	case SEQ_BACKWARD_SIZE: {
		const size_t in_start = *in_pos;

		const lzma_ret ret = lzma_vli_decode(&coder->tmp,
				&coder->pos, in, in_pos, in_size);

		const size_t in_used = *in_pos - in_start;

		if (update_size(&coder->total_size, in_used,
				coder->total_limit))
			return LZMA_DATA_ERROR;

		coder->size_of_backward_size += in_used;

		if (ret != LZMA_STREAM_END)
			return ret;

		if (coder->tmp != coder->total_size
				- coder->size_of_backward_size)
			return LZMA_DATA_ERROR;

		return_if_error(update_sequence(coder));

		break;
	}

	case SEQ_PADDING:
		if (in[*in_pos] == 0x00) {
			if (update_size(&coder->total_size, 1,
					coder->total_limit))
				return LZMA_DATA_ERROR;

			++*in_pos;
			break;
		}

		return update_sequence(coder);

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
block_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_block_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options)
{
	// This is pretty similar to lzma_block_encoder_init().
	// See comments there.

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_decode;
		next->end = &block_decoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	if (!lzma_vli_is_valid(options->total_size)
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
			|| options->header_size > options->total_size
			|| (options->handle_padding
				&& (options->has_uncompressed_size_in_footer
					|| options->has_backward_size)))
		return LZMA_PROG_ERROR;

	return_if_error(lzma_check_init(&next->coder->check, options->check));

	next->coder->sequence = SEQ_CODE;
	next->coder->options = options;
	next->coder->pos = 0;
	next->coder->total_size = options->header_size;
	next->coder->compressed_size = 0;
	next->coder->uncompressed_size = 0;
	next->coder->total_limit
			= MIN(options->total_size, options->total_limit);
	next->coder->uncompressed_limit = MIN(options->uncompressed_size,
			options->uncompressed_limit);
	next->coder->tmp = 0;
	next->coder->size_of_backward_size = 0;

	if (!options->has_eopm && options->uncompressed_size == 0) {
		// The Compressed Data field is empty, thus we skip SEQ_CODE
		// phase completely.
		const lzma_ret ret = update_sequence(next->coder);
		if (ret != LZMA_OK && ret != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;
	}

	return lzma_raw_decoder_init(&next->coder->next, allocator,
			options->filters, options->has_eopm
				? LZMA_VLI_VALUE_UNKNOWN
				: options->uncompressed_size,
			true);
}


extern LZMA_API lzma_ret
lzma_block_decoder(lzma_stream *strm, lzma_options_block *options)
{
	lzma_next_strm_init(strm, lzma_block_decoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
