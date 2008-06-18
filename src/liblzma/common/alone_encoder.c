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


#define ALONE_HEADER_SIZE (1 + 4 + 8)


struct lzma_coder_s {
	lzma_next_coder next;

	enum {
		SEQ_HEADER,
		SEQ_CODE,
	} sequence;

	size_t header_pos;
	uint8_t header[ALONE_HEADER_SIZE];
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
	case SEQ_HEADER:
		bufcpy(coder->header, &coder->header_pos,
				ALONE_HEADER_SIZE,
				out, out_pos, out_size);
		if (coder->header_pos < ALONE_HEADER_SIZE)
			return LZMA_OK;

		coder->sequence = SEQ_CODE;
		break;

	case SEQ_CODE:
		return coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

	default:
		assert(0);
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
		const lzma_options_lzma *options)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &alone_encode;
		next->end = &alone_encoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Basic initializations
	next->coder->sequence = SEQ_HEADER;
	next->coder->header_pos = 0;

	// Encode the header:
	// - Properties (1 byte)
	if (lzma_lzma_encode_properties(options, next->coder->header))
		return LZMA_PROG_ERROR;

	// - Dictionary size (4 bytes)
	if (options->dictionary_size < LZMA_DICTIONARY_SIZE_MIN
			|| options->dictionary_size > LZMA_DICTIONARY_SIZE_MAX)
		return LZMA_PROG_ERROR;

	// Round up to to the next 2^n or 2^n + 2^(n - 1) depending on which
	// one is the next. While the header would allow any 32-bit integer,
	// we do this to keep the decoder of liblzma accepting the resulting
	// files.
	uint32_t d = options->dictionary_size - 1;
	d |= d >> 2;
	d |= d >> 3;
	d |= d >> 4;
	d |= d >> 8;
	d |= d >> 16;
	++d;

	integer_write_32(next->coder->header + 1, d);

	// - Uncompressed size (always unknown and using EOPM)
	memset(next->coder->header + 1 + 4, 0xFF, 8);

	// Initialize the LZMA encoder.
	const lzma_filter_info filters[2] = {
		{
			.init = &lzma_lzma_encoder_init,
			.options = (void *)(options),
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
lzma_alone_encoder(lzma_stream *strm, const lzma_options_lzma *options)
{
	lzma_next_strm_init(strm, alone_encoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
