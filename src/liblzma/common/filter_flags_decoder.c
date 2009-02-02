///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_flags_decoder.c
/// \brief      Decodes a Filter Flags field
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

#include "filter_decoder.h"


extern LZMA_API(lzma_ret)
lzma_filter_flags_decode(
		lzma_filter *filter, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	// Set the pointer to NULL so the caller can always safely free it.
	filter->options = NULL;

	// Filter ID
	return_if_error(lzma_vli_decode(&filter->id, NULL,
			in, in_pos, in_size));

	if (filter->id >= LZMA_FILTER_RESERVED_START)
		return LZMA_DATA_ERROR;

	// Size of Properties
	lzma_vli props_size;
	return_if_error(lzma_vli_decode(&props_size, NULL,
			in, in_pos, in_size));

	// Filter Properties
	if (in_size - *in_pos < props_size)
		return LZMA_DATA_ERROR;

	const lzma_ret ret = lzma_properties_decode(
			filter, allocator, in + *in_pos, props_size);

	*in_pos += props_size;

	return ret;
}
