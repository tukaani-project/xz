///////////////////////////////////////////////////////////////////////////////
//
/// \file       simple_decoder.c
/// \brief      Properties decoder for simple filters
//
//  Copyright (C) 2007-2008 Lasse Collin
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

#include "simple_decoder.h"


extern lzma_ret
lzma_simple_props_decode(void **options, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size)
{
	if (props_size == 0)
		return LZMA_OK;

	if (props_size != 4)
		return LZMA_OPTIONS_ERROR;

	lzma_options_simple *opt = lzma_alloc(
			sizeof(lzma_options_simple), allocator);
	if (opt == NULL)
		return LZMA_MEM_ERROR;

	opt->start_offset = integer_read_32(props);

	// Don't leave an options structure allocated if start_offset is zero.
	if (opt->start_offset == 0)
		lzma_free(opt, allocator);
	else
		*options = opt;

	return LZMA_OK;
}
