///////////////////////////////////////////////////////////////////////////////
//
/// \file       simple_encoder.c
/// \brief      Properties encoder for simple filters
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

#include "simple_encoder.h"


extern lzma_ret
lzma_simple_props_size(uint32_t *size, const void *options)
{
	const lzma_options_bcj *const opt = options;
	*size = (opt == NULL || opt->start_offset == 0) ? 0 : 4;
	return LZMA_OK;
}


extern lzma_ret
lzma_simple_props_encode(const void *options, uint8_t *out)
{
	const lzma_options_bcj *const opt = options;

	// The default start offset is zero, so we don't need to store any
	// options unless the start offset is non-zero.
	if (opt == NULL || opt->start_offset == 0)
		return LZMA_OK;

	integer_write_32(out, opt->start_offset);

	return LZMA_OK;
}
