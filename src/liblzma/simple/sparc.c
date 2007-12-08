///////////////////////////////////////////////////////////////////////////////
//
/// \file       sparc.c
/// \brief      Filter for SPARC binaries
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
sparc_code(lzma_simple *simple lzma_attribute((unused)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	size_t i;
	for (i = 0; i + 4 <= size; i += 4) {

		if ((buffer[i] == 0x40 && (buffer[i + 1] & 0xC0) == 0x00)
				|| (buffer[i] == 0x7F
				&& (buffer[i + 1] & 0xC0) == 0xC0)) {

			uint32_t src = ((uint32_t)buffer[i + 0] << 24)
					| ((uint32_t)buffer[i + 1] << 16)
					| ((uint32_t)buffer[i + 2] << 8)
					| ((uint32_t)buffer[i + 3]);

			src <<= 2;

			uint32_t dest;
			if (is_encoder)
				dest = now_pos + (uint32_t)(i) + src;
			else
				dest = src - (now_pos + (uint32_t)(i));

			dest >>= 2;

			dest = (((0 - ((dest >> 22) & 1)) << 22) & 0x3FFFFFFF)
					| (dest & 0x3FFFFF)
					| 0x40000000;

			buffer[i + 0] = (uint8_t)(dest >> 24);
			buffer[i + 1] = (uint8_t)(dest >> 16);
			buffer[i + 2] = (uint8_t)(dest >> 8);
			buffer[i + 3] = (uint8_t)(dest);
		}
	}

	return i;
}


static lzma_ret
sparc_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&sparc_code, 0, 4, is_encoder);
}


extern lzma_ret
lzma_simple_sparc_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	return sparc_coder_init(next, allocator, filters, true);
}


extern lzma_ret
lzma_simple_sparc_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	return sparc_coder_init(next, allocator, filters, false);
}
