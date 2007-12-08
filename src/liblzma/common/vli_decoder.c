///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_decoder.c
/// \brief      Decodes variable-length integers
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
lzma_vli_decode(lzma_vli *restrict vli, size_t *restrict vli_pos,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size)
{
	if (*vli > LZMA_VLI_VALUE_MAX || *vli_pos >= 9
			|| (*vli >> (7 * *vli_pos)) != 0)
		return LZMA_PROG_ERROR;

	if (*in_pos >= in_size)
		return LZMA_BUF_ERROR;

	if (*vli_pos == 0) {
		*vli_pos = 1;

		if (in[*in_pos] <= 0x7F) {
			// Single-byte integer
			*vli = in[*in_pos];
			++*in_pos;
			return LZMA_STREAM_END;
		}

		*vli = in[*in_pos] & 0x7F;
		++*in_pos;
	}

	while (*in_pos < in_size) {
		// Read in the next byte.
		*vli |= (lzma_vli)(in[*in_pos] & 0x7F) << (*vli_pos * 7);
		++*vli_pos;

		// Check if this is the last byte of a multibyte integer.
		if (in[*in_pos] & 0x80) {
			++*in_pos;
			return LZMA_STREAM_END;
		}

		// Limit variable-length representation to nine bytes.
		if (*vli_pos == 9)
			return LZMA_DATA_ERROR;

		// Increment input position only when the byte was accepted.
		++*in_pos;
	}

	return LZMA_OK;
}
