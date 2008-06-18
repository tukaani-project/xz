///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_header.c
/// \brief      Utility functions to handle lzma_options_block
//
//  Copyright (C) 2008 Lasse Collin
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


extern LZMA_API lzma_ret
lzma_block_total_size_set(lzma_options_block *options, lzma_vli total_size)
{
	// Validate.
	if (options->header_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| options->header_size > LZMA_BLOCK_HEADER_SIZE_MAX
			|| (options->header_size & 3)
			|| (unsigned)(options->check) > LZMA_CHECK_ID_MAX
			|| (total_size & 3))
		return LZMA_PROG_ERROR;

	const uint32_t container_size = options->header_size
			+ lzma_check_sizes[options->check];

	// Validate that Compressed Size will be greater than zero.
	if (container_size <= total_size)
		return LZMA_DATA_ERROR;

	options->compressed_size = total_size - container_size;

	return LZMA_OK;
}


extern LZMA_API lzma_vli
lzma_block_total_size_get(const lzma_options_block *options)
{
	// Validate the values that we are interested in.
	if (options->header_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| options->header_size > LZMA_BLOCK_HEADER_SIZE_MAX
			|| (options->header_size & 3)
			|| (unsigned)(options->check) > LZMA_CHECK_ID_MAX)
		return 0;

	// If Compressed Size is unknown, return that we cannot know
	// Total Size either.
	if (options->compressed_size == LZMA_VLI_VALUE_UNKNOWN)
		return LZMA_VLI_VALUE_UNKNOWN;

	const lzma_vli total_size = options->compressed_size
			+ options->header_size
			+ lzma_check_sizes[options->check];

	// Validate the calculated Total Size.
	if (options->compressed_size > LZMA_VLI_VALUE_MAX
			|| (options->compressed_size & 3)
			|| total_size > LZMA_VLI_VALUE_MAX)
		return 0;

	return total_size;
}
