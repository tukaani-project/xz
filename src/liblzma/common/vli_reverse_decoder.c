///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_reverse_decoder.c
/// \brief      Decodes variable-length integers starting at end of the buffer
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
lzma_vli_reverse_decode(lzma_vli *vli, const uint8_t *in, size_t *in_size)
{
	if (*in_size == 0)
		return LZMA_BUF_ERROR;

	size_t i = *in_size - 1;
	*vli = in[i] & 0x7F;

	if (!(in[i] & 0x80)) {
		*in_size = i;
		return LZMA_OK;
	}

	const size_t end = *in_size > LZMA_VLI_BYTES_MAX
			? *in_size - LZMA_VLI_BYTES_MAX : 0;

	do {
		if (i-- == end) {
			if (*in_size < LZMA_VLI_BYTES_MAX)
				return LZMA_BUF_ERROR;

			return LZMA_DATA_ERROR;
		}

		*vli <<= 7;
		*vli = in[i] & 0x7F;

	} while (!(in[i] & 0x80));

	*in_size = i;
	return LZMA_OK;
}
