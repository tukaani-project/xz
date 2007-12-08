///////////////////////////////////////////////////////////////////////////////
//
/// \file       armthumb.c
/// \brief      Filter for ARM-Thumb binaries
//
//  Copyright (C) 1999-2006 Igor Pavlov
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

#include "simple_private.h"


static size_t
armthumb_code(lzma_simple *simple lzma_attribute((unused)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	uint32_t i;
	for (i = 0; i + 4 <= size; i += 2) {
		if ((buffer[i + 1] & 0xF8) == 0xF0
				&& (buffer[i + 3] & 0xF8) == 0xF8) {
			uint32_t src = ((buffer[i + 1] & 0x7) << 19)
					| (buffer[i + 0] << 11)
					| ((buffer[i + 3] & 0x7) << 8)
					| (buffer[i + 2]);

			src <<= 1;

			uint32_t dest;
			if (is_encoder)
				dest = now_pos + (uint32_t)(i) + 4 + src;
			else
				dest = src - (now_pos + (uint32_t)(i) + 4);

			dest >>= 1;
			buffer[i + 1] = 0xF0 | ((dest >> 19) & 0x7);
			buffer[i + 0] = (dest >> 11);
			buffer[i + 3] = 0xF8 | ((dest >> 8) & 0x7);
			buffer[i + 2] = (dest);
			i += 2;
		}
	}

	return i;
}


static lzma_ret
armthumb_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&armthumb_code, 0, 4, is_encoder);
}


extern lzma_ret
lzma_simple_armthumb_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	return armthumb_coder_init(next, allocator, filters, true);
}


extern lzma_ret
lzma_simple_armthumb_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	return armthumb_coder_init(next, allocator, filters, false);
}
