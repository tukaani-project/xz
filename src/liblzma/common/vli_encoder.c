///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_encoder.c
/// \brief      Encodes variable-length integers
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


extern LZMA_API lzma_ret
lzma_vli_encode(lzma_vli vli, size_t *restrict vli_pos, size_t vli_size,
		uint8_t *restrict out, size_t *restrict out_pos,
		size_t out_size)
{
	if (vli > LZMA_VLI_VALUE_MAX || *vli_pos >= 9 || vli_size > 9
			|| (vli != 0 && (vli >> (7 * *vli_pos)) == 0))
		return LZMA_PROG_ERROR;

	if (*out_pos >= out_size)
		return LZMA_BUF_ERROR;

	if (*vli_pos == 0) {
		*vli_pos = 1;

		if (vli <= 0x7F && *vli_pos >= vli_size) {
			// Single-byte integer
			out[(*out_pos)++] = vli;
			return LZMA_STREAM_END;
		}

		// First byte of a multibyte integer
		out[(*out_pos)++] = (vli & 0x7F) | 0x80;
	}

	while (*out_pos < out_size) {
		const lzma_vli b = vli >> (7 * *vli_pos);
		++*vli_pos;

		if (b <= 0x7F && *vli_pos >= vli_size) {
			// Last byte of a multibyte integer
			out[(*out_pos)++] = (b & 0xFF) | 0x80;
			return LZMA_STREAM_END;
		}

		// Middle byte of a multibyte integer
		out[(*out_pos)++] = b & 0x7F;
	}

	// vli is not yet completely written out.
	return LZMA_OK;
}


extern LZMA_API size_t
lzma_vli_size(lzma_vli vli)
{
	if (vli > LZMA_VLI_VALUE_MAX)
		return 0;

	size_t i = 0;
	do {
		vli >>= 7;
		++i;
	} while (vli != 0);

	assert(i <= 9);
	return i;
}
