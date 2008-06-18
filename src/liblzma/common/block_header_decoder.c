///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_header_decoder.c
/// \brief      Decodes Block Header from .lzma files
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


static void
free_properties(lzma_options_block *options, lzma_allocator *allocator)
{
	// Free allocated filter options. The last array member is not
	// touched after the initialization in the beginning of
	// lzma_block_header_decode(), so we don't need to touch that here.
	for (size_t i = 0; i < LZMA_BLOCK_FILTERS_MAX; ++i) {
		lzma_free(options->filters[i].options, allocator);
		options->filters[i].id = LZMA_VLI_VALUE_UNKNOWN;
		options->filters[i].options = NULL;
	}

	return;
}


extern LZMA_API lzma_ret
lzma_block_header_decode(lzma_options_block *options,
		lzma_allocator *allocator, const uint8_t *in)
{
	// NOTE: We consider the header to be corrupt not only when the
	// CRC32 doesn't match, but also when variable-length integers
	// are invalid or not over 63 bits, or if the header is too small
	// to contain the claimed information.

	// Initialize the filter options array. This way the caller can
	// safely free() the options even if an error occurs in this function.
	for (size_t i = 0; i <= LZMA_BLOCK_FILTERS_MAX; ++i) {
		options->filters[i].id = LZMA_VLI_VALUE_UNKNOWN;
		options->filters[i].options = NULL;
	}

	size_t in_size = options->header_size;

	// Validate. The caller must have set options->header_size with
	// lzma_block_header_size_decode() macro, so it is a programming error
	// if these tests fail.
	if (in_size < LZMA_BLOCK_HEADER_SIZE_MIN
			|| in_size > LZMA_BLOCK_HEADER_SIZE_MAX
			|| (in_size & 3)
			|| lzma_block_header_size_decode(in[0]) != in_size)
		return LZMA_PROG_ERROR;

	// Exclude the CRC32 field.
	in_size -= 4;

	// Verify CRC32
	if (lzma_crc32(in, in_size, 0) != integer_read_32(in + in_size))
		return LZMA_DATA_ERROR;

	// Check for unsupported flags.
	if (in[1] & 0x3C)
		return LZMA_HEADER_ERROR;

	// Start after the Block Header Size and Block Flags fields.
	size_t in_pos = 2;

	// Compressed Size
	if (in[1] & 0x40) {
		return_if_error(lzma_vli_decode(&options->compressed_size,
				NULL, in, &in_pos, in_size));

		if (options->compressed_size > LZMA_VLI_VALUE_MAX / 4 - 1)
			return LZMA_DATA_ERROR;

		options->compressed_size = (options->compressed_size + 1) * 4;

		// Check that Total Size (that is, size of
		// Block Header + Compressed Data + Check) is
		// representable as a VLI.
		if (lzma_block_total_size_get(options) == 0)
			return LZMA_DATA_ERROR;
	} else {
		options->compressed_size = LZMA_VLI_VALUE_UNKNOWN;
	}

	// Uncompressed Size
	if (in[1] & 0x80)
		return_if_error(lzma_vli_decode(&options->uncompressed_size,
				NULL, in, &in_pos, in_size));
	else
		options->uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;

	// Filter Flags
	const size_t filter_count = (in[1] & 3) + 1;
	for (size_t i = 0; i < filter_count; ++i) {
		const lzma_ret ret = lzma_filter_flags_decode(
				&options->filters[i], allocator,
				in, &in_pos, in_size);
		if (ret != LZMA_OK) {
			free_properties(options, allocator);
			return ret;
		}
	}

	// Padding
	while (in_pos < in_size) {
		if (in[in_pos++] != 0x00) {
			free_properties(options, allocator);

			// Possibly some new field present so use
			// LZMA_HEADER_ERROR instead of LZMA_DATA_ERROR.
			return LZMA_HEADER_ERROR;
		}
	}

	return LZMA_OK;
}
