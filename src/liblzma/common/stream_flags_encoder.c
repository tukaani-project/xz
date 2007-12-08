///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_flags_encoder.c
/// \brief      Encodes Stream Header and Footer for .lzma files
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

#include "stream_common.h"


static bool
stream_flags_encode(uint8_t *flags_byte, const lzma_stream_flags *options)
{
	// Check type
	if ((unsigned int)(options->check) > LZMA_CHECK_ID_MAX)
		return true;

	*flags_byte = options->check;

	// Usage of CRC32 in Block Headers
	if (options->has_crc32)
		*flags_byte |= 0x08;

	// Single- or Multi-Block
	if (options->is_multi)
		*flags_byte |= 0x10;

	return false;
}


extern LZMA_API lzma_ret
lzma_stream_header_encode(uint8_t *out, const lzma_stream_flags *options)
{
	// Magic
	memcpy(out, lzma_header_magic, sizeof(lzma_header_magic));

	// Stream Flags
	if (stream_flags_encode(out + sizeof(lzma_header_magic), options))
		return LZMA_PROG_ERROR;;

	// CRC32 of the Stream Header
	const uint32_t crc = lzma_crc32(out + sizeof(lzma_header_magic), 1, 0);

	for (size_t i = 0; i < 4; ++i)
		out[sizeof(lzma_header_magic) + 1 + i] = crc >> (i * 8);

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_stream_tail_encode(uint8_t *out, const lzma_stream_flags *options)
{
	// Stream Flags
	if (stream_flags_encode(out, options))
		return LZMA_PROG_ERROR;

	// Magic
	memcpy(out + 1, lzma_footer_magic, sizeof(lzma_footer_magic));

	return LZMA_OK;
}
