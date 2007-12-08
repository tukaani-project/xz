///////////////////////////////////////////////////////////////////////////////
//
/// \file       alone_decoder.c
/// \brief      Decoder for LZMA_Alone files
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

#include "common.h"
#include "lzma_encoder.h"


struct lzma_coder_s {
	lzma_next_coder next;

	enum {
		SEQ_PROPERTIES,
		SEQ_DICTIONARY_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_CODE,
	} sequence;

	size_t pos;

	lzma_options_alone options;
};


static lzma_ret
alone_encode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action)
{
	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_PROPERTIES:
		if (lzma_lzma_encode_properties(
				&coder->options.lzma, out + *out_pos)) {
			return LZMA_PROG_ERROR;
		}

		coder->sequence = SEQ_DICTIONARY_SIZE;
		++*out_pos;
		break;

	case SEQ_DICTIONARY_SIZE:
		out[*out_pos] = coder->options.lzma.dictionary_size
				>> (coder->pos * 8);

		if (++coder->pos == 4) {
			coder->pos = 0;
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		}

		++*out_pos;
		break;

	case SEQ_UNCOMPRESSED_SIZE:
		out[*out_pos] = coder->options.uncompressed_size
				>> (coder->pos * 8);

		if (++coder->pos == 8) {
			coder->pos = 0;
			coder->sequence = SEQ_CODE;
		}

		++*out_pos;
		break;

	case SEQ_CODE: {
		return coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
alone_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


// At least for now, this is not used by any internal function.
static lzma_ret
alone_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_alone *options)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &alone_encode;
		next->end = &alone_encoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Initialize the LZMA_Alone coder variables.
	next->coder->sequence = SEQ_PROPERTIES;
	next->coder->pos = 0;
	next->coder->options = *options;

	// Verify uncompressed_size since the other functions assume that
	// it is valid.
	if (!lzma_vli_is_valid(next->coder->options.uncompressed_size))
		return LZMA_PROG_ERROR;

	// Initialize the LZMA encoder.
	const lzma_filter_info filters[2] = {
		{
			.init = &lzma_lzma_encoder_init,
			.options = &next->coder->options.lzma,
			.uncompressed_size = next->coder->options
					.uncompressed_size,
		}, {
			.init = NULL,
		}
	};

	return lzma_next_filter_init(&next->coder->next, allocator, filters);
}


/*
extern lzma_ret
lzma_alone_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_alone *options)
{
	lzma_next_coder_init(alone_encoder_init, next, allocator, options);
}
*/


extern LZMA_API lzma_ret
lzma_alone_encoder(lzma_stream *strm, const lzma_options_alone *options)
{
	lzma_next_strm_init(strm, alone_encoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
