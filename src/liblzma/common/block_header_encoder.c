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
	// Block Flags take two bytes.
	size_t size = 2;

	// Compressed Size
	if (!lzma_vli_is_valid(options->compressed_size)) {
		return LZMA_PROG_ERROR;

	} else if (options->compressed_reserve != 0) {
		// Make sure that the known Compressed Size fits into the
		// reserved space. Note that lzma_vli_size() will return zero
		// if options->compressed_size is LZMA_VLI_VALUE_UNKNOWN, so
		// we don't need to handle that special case separately.
		if (options->compressed_reserve > LZMA_VLI_BYTES_MAX
				|| lzma_vli_size(options->compressed_size)
				> (size_t)(options->compressed_reserve))
			return LZMA_PROG_ERROR;

		size += options->compressed_reserve;

	} else if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		// Compressed Size is known. We have already checked
		// that is is a valid VLI, and since it isn't
		// LZMA_VLI_VALUE_UNKNOWN, we can be sure that
		// lzma_vli_size() will succeed.
		size += lzma_vli_size(options->compressed_size);
	}

	// Uncompressed Size
	if (!lzma_vli_is_valid(options->uncompressed_size)) {
		return LZMA_PROG_ERROR;

	} else if (options->uncompressed_reserve != 0) {
		if (options->uncompressed_reserve > LZMA_VLI_BYTES_MAX
				|| lzma_vli_size(options->uncompressed_size)
				> (size_t)(options->uncompressed_reserve))
			return LZMA_PROG_ERROR;

		size += options->uncompressed_reserve;

	} else if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		size += lzma_vli_size(options->uncompressed_size);
	}

	// List of Filter Flags
	for (size_t i = 0; options->filters[i].id != LZMA_VLI_VALUE_UNKNOWN;
			++i) {
		// Don't allow too many filters.
		if (i == 7)
			return LZMA_PROG_ERROR;

		uint32_t tmp;
		const lzma_ret ret = lzma_filter_flags_size(&tmp,
				options->filters + i);
		if (ret != LZMA_OK)
			return ret;

		size += tmp;
	}

	// CRC32
	if (options->has_crc32)
		size += 4;

	// Padding
	int32_t padding;
	if (options->padding == LZMA_BLOCK_HEADER_PADDING_AUTO) {
		const uint32_t preferred = lzma_alignment_output(
				options->filters, 1);
		const uint32_t unaligned = size + options->alignment;
		padding = (int32_t)(unaligned % preferred);
		if (padding != 0)
			padding = preferred - padding;
	} else if (options->padding >= LZMA_BLOCK_HEADER_PADDING_MIN
			&& options->padding <= LZMA_BLOCK_HEADER_PADDING_MAX) {
		padding = options->padding;
	} else {
		return LZMA_PROG_ERROR;
	}

	// All success. Copy the calculated values to the options structure.
	options->padding = padding;
	options->header_size = size + (size_t)(padding);

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_block_header_encode(uint8_t *out, const lzma_options_block *options)
{
	// We write the Block Flags later.
	if (options->header_size < 2)
		return LZMA_PROG_ERROR;

	const size_t out_size = options->header_size;
	size_t out_pos = 2;

	// Compressed Size
	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN
			|| options->compressed_reserve != 0) {
		const lzma_vli size = options->compressed_size
					!= LZMA_VLI_VALUE_UNKNOWN
				? options->compressed_size : 0;
		size_t vli_pos = 0;
		if (lzma_vli_encode(
				size, &vli_pos, options->compressed_reserve,
				out, &out_pos, out_size) != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;

	}

	// Uncompressed Size
	if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN
			|| options->uncompressed_reserve != 0) {
		const lzma_vli size = options->uncompressed_size
					!= LZMA_VLI_VALUE_UNKNOWN
				? options->uncompressed_size : 0;
		size_t vli_pos = 0;
		if (lzma_vli_encode(
				size, &vli_pos, options->uncompressed_reserve,
				out, &out_pos, out_size) != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;

	}

	// Filter Flags
	size_t filter_count;
	for (filter_count = 0; options->filters[filter_count].id
			!= LZMA_VLI_VALUE_UNKNOWN; ++filter_count) {
		// There can be at maximum of seven filters.
		if (filter_count == 7)
			return LZMA_PROG_ERROR;

		const lzma_ret ret = lzma_filter_flags_encode(out, &out_pos,
				out_size, options->filters + filter_count);
		// FIXME: Don't return LZMA_BUF_ERROR.
		if (ret != LZMA_OK)
			return ret;
	}

	// Block Flags 1
	out[0] = filter_count;

	if (options->has_eopm)
		out[0] |= 0x08;
	else if (options->uncompressed_size == LZMA_VLI_VALUE_UNKNOWN)
		return LZMA_PROG_ERROR;

	if (options->compressed_size != LZMA_VLI_VALUE_UNKNOWN
			|| options->compressed_reserve != 0)
		out[0] |= 0x10;

	if (options->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN
			|| options->uncompressed_reserve != 0)
		out[0] |= 0x20;

	if (options->is_metadata)
		out[0] |= 0x80;

	// Block Flags 2
	if (options->padding < LZMA_BLOCK_HEADER_PADDING_MIN
			|| options->padding > LZMA_BLOCK_HEADER_PADDING_MAX)
		return LZMA_PROG_ERROR;

	out[1] = (uint8_t)(options->padding);

	// CRC32
	if (options->has_crc32) {
		if (out_size - out_pos < 4)
			return LZMA_PROG_ERROR;

		const uint32_t crc = lzma_crc32(out, out_pos, 0);
		for (size_t i = 0; i < 4; ++i)
			out[out_pos++] = crc >> (i * 8);
	}

	// Padding - the amount of available space must now match with
	// the size of the Padding field.
	if (out_size - out_pos != (size_t)(options->padding))
		return LZMA_PROG_ERROR;

	memzero(out + out_pos, (size_t)(options->padding));

	return LZMA_OK;
}
