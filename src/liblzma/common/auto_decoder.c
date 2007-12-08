///////////////////////////////////////////////////////////////////////////////
//
/// \file       auto_decoder.c
/// \brief      Autodetect between .lzma Stream and LZMA_Alone formats
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
#include "alone_decoder.h"


struct lzma_coder_s {
	lzma_next_coder next;

	lzma_extra **header;
	lzma_extra **footer;
	bool initialized;
};


static lzma_ret
auto_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	if (!coder->initialized) {
		if (*in_pos >= in_size)
			return LZMA_OK;

		lzma_ret ret;

		if (in[*in_pos] == 0xFF)
			ret = lzma_stream_decoder_init(&coder->next, allocator,
					coder->header, coder->footer);
		else
			ret = lzma_alone_decoder_init(&coder->next, allocator);

		if (ret != LZMA_OK)
			return ret;

		coder->initialized = true;
	}

	return coder->next.code(coder->next.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size, action);
}


static void
auto_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
auto_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_extra **header, lzma_extra **footer)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &auto_decode;
		next->end = &auto_decoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	next->coder->header = header;
	next->coder->footer = footer;
	next->coder->initialized = false;

	return LZMA_OK;
}


/*
extern lzma_ret
lzma_auto_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_extra **header, lzma_extra **footer)
{
	lzma_next_coder_init(
			auto_decoder_init, next, allocator, header, footer);
}
*/


extern LZMA_API lzma_ret
lzma_auto_decoder(lzma_stream *strm, lzma_extra **header, lzma_extra **footer)
{
	lzma_next_strm_init(strm, auto_decoder_init, header, footer);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
