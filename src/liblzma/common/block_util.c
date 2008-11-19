///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_header.c
/// \brief      Utility functions to handle lzma_block
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
#include "index.h"


extern LZMA_API lzma_ret
lzma_block_compressed_size(lzma_block *options, lzma_vli total_size)
{
	// Validate.
	if (options->header_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| options->header_size > LZMA_BLOCK_HEADER_SIZE_MAX
			|| (options->header_size & 3)
			|| (unsigned)(options->check) > LZMA_CHECK_ID_MAX
			|| (total_size & 3))
		return LZMA_PROG_ERROR;

	const uint32_t container_size = options->header_size
			+ lzma_check_size(options->check);

	// Validate that Compressed Size will be greater than zero.
	if (container_size <= total_size)
		return LZMA_DATA_ERROR;

	options->compressed_size = total_size - container_size;

	return LZMA_OK;
}


extern LZMA_API lzma_vli
lzma_block_unpadded_size(const lzma_block *options)
{
	// Validate the values that we are interested in i.e. all but
	// Uncompressed Size and the filters.
	//
	// NOTE: This function is used for validation too, so it is
	// essential that these checks are always done even if
	// Compressed Size is unknown.
	if (options->header_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| options->header_size > LZMA_BLOCK_HEADER_SIZE_MAX
			|| (options->header_size & 3)
			|| !lzma_vli_is_valid(options->compressed_size)
			|| options->compressed_size == 0
			|| (unsigned int)(options->check) > LZMA_CHECK_ID_MAX)
		return 0;

	// If Compressed Size is unknown, return that we cannot know
	// size of the Block either.
	if (options->compressed_size == LZMA_VLI_UNKNOWN)
		return LZMA_VLI_UNKNOWN;

	// Calculate Unpadded Size and validate it.
	const lzma_vli unpadded_size = options->compressed_size
				+ options->header_size
				+ lzma_check_size(options->check);

	assert(unpadded_size >= UNPADDED_SIZE_MIN);
	if (unpadded_size > UNPADDED_SIZE_MAX)
		return 0;

	return unpadded_size;
}


extern LZMA_API lzma_vli
lzma_block_total_size(const lzma_block *options)
{
	lzma_vli unpadded_size = lzma_block_unpadded_size(options);

	if (unpadded_size != 0 && unpadded_size != LZMA_VLI_UNKNOWN)
		unpadded_size = vli_ceil4(unpadded_size);

	return unpadded_size;
}
