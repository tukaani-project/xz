///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_encoder.c
/// \brief      Encodes variable-length integers
//
//  Copyright (C) 2007-2008 Lasse Collin
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
lzma_vli_encode(lzma_vli vli, size_t *restrict vli_pos,
		uint8_t *restrict out, size_t *restrict out_pos,
		size_t out_size)
{
	// If we haven't been given vli_pos, work in single-call mode.
	size_t vli_pos_internal = 0;
	if (vli_pos == NULL)
		vli_pos = &vli_pos_internal;

	// Validate the arguments.
	if (*vli_pos >= LZMA_VLI_BYTES_MAX || vli > LZMA_VLI_MAX)
		return LZMA_PROG_ERROR;

	if (*out_pos >= out_size)
		return LZMA_BUF_ERROR;

	// Write the non-last bytes in a loop.
	while ((vli >> (*vli_pos * 7)) >= 0x80) {
		out[*out_pos] = (uint8_t)(vli >> (*vli_pos * 7)) | 0x80;

		++*vli_pos;
		assert(*vli_pos < LZMA_VLI_BYTES_MAX);

		if (++*out_pos == out_size)
			return vli_pos == &vli_pos_internal
					? LZMA_PROG_ERROR : LZMA_OK;
	}

	// Write the last byte.
	out[*out_pos] = (uint8_t)(vli >> (*vli_pos * 7));
	++*out_pos;
	++*vli_pos;

	return vli_pos == &vli_pos_internal ? LZMA_OK : LZMA_STREAM_END;

}
