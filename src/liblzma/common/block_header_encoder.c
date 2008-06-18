///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_header_encoder.c
/// \brief      Encodes Block Header for .lzma files
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

#include "common.h"
#include "check.h"


extern LZMA_API lzma_ret
lzma_block_header_size(lzma_options_block *options)
{
	// Block Header Size + Block Flags + CRC32.
	size_t size = 1 + 1 + 4;

	// Compressed Size
	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (options->compressed_size > LZMA_VLI_VALUE_MAX / 4 - 1
				|| options->compressed_size == 0
				|| (options->compressed_size & 3))
			return LZMA_PROG_ERROR;

		size += lzma_vli_size(options->compressed_size / 4 - 1);
	}

	// Uncompressed Size
	if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		const size_t add = lzma_vli_size(options->uncompressed_size);
		if (add == 0)
			return LZMA_PROG_ERROR;

		size += add;
	}

	// List of Filter Flags
	if (options->filters == NULL
			|| options->filters[0].id == LZMA_VLI_VALUE_UNKNOWN)
		return LZMA_PROG_ERROR;

	for (size_t i = 0; options->filters[i].id != LZMA_VLI_VALUE_UNKNOWN;
			++i) {
		// Don't allow too many filters.
		if (i == 4)
			return LZMA_PROG_ERROR;

		uint32_t add;
		return_if_error(lzma_filter_flags_size(&add,
				options->filters + i));

		size += add;
	}

	// Pad to a multiple of four bytes.
	options->header_size = (size + 3) & ~(size_t)(3);

	// NOTE: We don't verify that Total Size of the Block stays within
	// limits. This is because it is possible that we are called with
	// exaggerated values to reserve space for Block Header, and later
	// called again with lower, real values.

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_block_header_encode(const lzma_options_block *options, uint8_t *out)
{
	if ((options->header_size & 3)
			|| options->header_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| options->header_size > LZMA_BLOCK_HEADER_SIZE_MAX)
		return LZMA_PROG_ERROR;

	// Indicate the size of the buffer _excluding_ the CRC32 field.
	const size_t out_size = options->header_size - 4;

	// Store the Block Header Size.
	out[0] = out_size / 4;

	// We write Block Flags a little later.
	size_t out_pos = 2;

	// Compressed Size
	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		// Compressed Size must be non-zero, fit into a 63-bit
		// integer and be a multiple of four. Also the Total Size
		// of the Block must fit into 63-bit integer.
		if (options->compressed_size == 0
				|| (options->compressed_size & 3)
				|| options->compressed_size
					> LZMA_VLI_VALUE_MAX
				|| lzma_block_total_size_get(options) == 0)
			return LZMA_PROG_ERROR;

		return_if_error(lzma_vli_encode(
				options->compressed_size / 4 - 1, NULL,
				out, &out_pos, out_size));
	}

	// Uncompressed Size
	if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		return_if_error(lzma_vli_encode(
				options->uncompressed_size, NULL,
				out, &out_pos, out_size));

	// Filter Flags
	if (options->filters == NULL
			|| options->filters[0].id == LZMA_VLI_VALUE_UNKNOWN)
		return LZMA_PROG_ERROR;

	size_t filter_count = 0;
	do {
		// There can be at maximum of four filters.
		if (filter_count == 4)
			return LZMA_PROG_ERROR;

		return_if_error(lzma_filter_flags_encode(out, &out_pos,
				out_size, options->filters + filter_count));

	} while (options->filters[++filter_count].id
			!= LZMA_VLI_VALUE_UNKNOWN);

	// Block Flags
	out[1] = filter_count - 1;

	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN)
		out[1] |= 0x40;

	if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		out[1] |= 0x80;

	// Padding
	memzero(out + out_pos, out_size - out_pos);

	// CRC32
	integer_write_32(out + out_size, lzma_crc32(out, out_size, 0));

	return LZMA_OK;
}
