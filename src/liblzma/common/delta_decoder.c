///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_decoder.c
/// \brief      Delta filter decoder
//
//  Copyright (C) 2007, 2008 Lasse Collin
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

#include "delta_decoder.h"
#include "delta_common.h"


static void
decode_buffer(lzma_coder *coder, uint8_t *buffer, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		buffer[i] += coder->history[(distance + coder->pos) & 0xFF];
		coder->history[coder->pos-- & 0xFF] = buffer[i];
	}
}


static lzma_ret
delta_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	assert(coder->next.code != NULL);

	const size_t out_start = *out_pos;

	const lzma_ret ret = coder->next.code(coder->next.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size,
			action);

	decode_buffer(coder, out + out_start, *out_pos - out_start);

	return ret;
}


extern lzma_ret
lzma_delta_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return lzma_delta_coder_init(next, allocator, filters, &delta_decode);
}
