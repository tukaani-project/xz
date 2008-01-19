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


/// Copies and decodes the data at the same time. This is used when Delta
/// is the last filter in the chain.
static void
copy_and_decode(lzma_coder *coder,
		const uint8_t *restrict in, uint8_t *restrict out, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		out[i] = in[i] + coder->history[
				(distance + coder->pos) & 0xFF];
		coder->history[coder->pos-- & 0xFF] = out[i];
	}
}


/// Decodes the data in place. This is used when we are not the last filter
/// in the chain.
static void
decode_in_place(lzma_coder *coder, uint8_t *buffer, size_t size)
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
	lzma_ret ret;

	if (coder->next.code == NULL) {
		// Limit in_size so that we don't copy too much.
		if ((lzma_vli)(in_size - *in_pos) > coder->uncompressed_size)
			in_size = *in_pos + (size_t)(coder->uncompressed_size);

		const size_t in_avail = in_size - *in_pos;
		const size_t out_avail = out_size - *out_pos;
		const size_t size = MIN(in_avail, out_avail);

		copy_and_decode(coder, in + *in_pos, out + *out_pos, size);

		*in_pos += size;
		*out_pos += size;

		assert(coder->uncompressed_size <= LZMA_VLI_VALUE_MAX);
		coder->uncompressed_size -= size;

		ret = coder->uncompressed_size == 0
				? LZMA_STREAM_END : LZMA_OK;

	} else {
		const size_t out_start = *out_pos;

		ret = coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

		decode_in_place(coder, out + out_start, *out_pos - out_start);
	}

	return ret;
}


extern lzma_ret
lzma_delta_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return lzma_delta_coder_init(next, allocator, filters, &delta_decode);
}
