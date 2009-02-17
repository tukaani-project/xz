///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_buffer_encoder.c
/// \brief      Easy single-call .xz Stream encoder
//
//  Copyright (C) 2009 Lasse Collin
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

#include "easy_preset.h"


extern LZMA_API(lzma_ret)
lzma_easy_buffer_encode(uint32_t preset, lzma_check check,
		lzma_allocator *allocator, const uint8_t *in, size_t in_size,
		uint8_t *out, size_t *out_pos, size_t out_size)
{
	lzma_options_easy opt_easy;
	if (lzma_easy_preset(&opt_easy, preset))
		return LZMA_OPTIONS_ERROR;

	return lzma_stream_buffer_encode(opt_easy.filters, check,
			allocator, in, in_size, out, out_pos, out_size);
}
