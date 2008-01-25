///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_private.h
/// \brief      Common stuff for Block encoder and decoder
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

#ifndef LZMA_BLOCK_COMMON_H
#define LZMA_BLOCK_COMMON_H

#include "common.h"

static inline bool
update_size(lzma_vli *size, lzma_vli add, lzma_vli limit)
{
	if (limit > LZMA_VLI_VALUE_MAX)
		limit = LZMA_VLI_VALUE_MAX;

	if (limit < *size || limit - *size < add)
		return true;

	*size += add;

	return false;
}


static inline bool
is_size_valid(lzma_vli size, lzma_vli reference)
{
	return reference == LZMA_VLI_VALUE_UNKNOWN || reference == size;
}


/// If any of these tests fail, the caller has to return LZMA_PROG_ERROR.
static inline bool
validate_options_1(const lzma_options_block *options)
{
	return options == NULL
			|| !lzma_vli_is_valid(options->compressed_size)
			|| !lzma_vli_is_valid(options->uncompressed_size)
			|| !lzma_vli_is_valid(options->total_size)
			|| !lzma_vli_is_valid(options->total_limit)
			|| !lzma_vli_is_valid(options->uncompressed_limit);
}


/// If any of these tests fail, the encoder has to return LZMA_PROG_ERROR
/// because something is going horribly wrong if such values get passed
/// to the encoder. In contrast, the decoder has to return LZMA_DATA_ERROR,
/// since these tests failing indicate that something is wrong in the Stream.
static inline bool
validate_options_2(const lzma_options_block *options)
{
	if ((options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN
				&& options->uncompressed_size
					> options->uncompressed_limit)
			|| (options->total_size != LZMA_VLI_VALUE_UNKNOWN
				&& options->total_size
					> options->total_limit)
			|| (!options->has_eopm && options->uncompressed_size
				== LZMA_VLI_VALUE_UNKNOWN)
			|| options->header_size > options->total_size)
		return true;

	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		// Calculate a rough minimum possible valid Total Size of
		// this Block, and check that total_size and total_limit
		// are big enough. Note that the real minimum size can be
		// bigger due to the Check, Uncompressed Size, Backwards
		// Size, pr Padding being present. A rough check here is
		// enough for us to catch the most obvious errors as early
		// as possible.
		const lzma_vli total_min = options->compressed_size
				+ (lzma_vli)(options->header_size);
		if (total_min > options->total_size
				|| total_min > options->total_limit)
			return true;
	}

	return false;
}

#endif
