///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_decoder.c
/// \brief      Decodes variable-length integers
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
lzma_vli_decode(lzma_vli *restrict vli, size_t *restrict vli_pos,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size)
{
	// If we haven't been given vli_pos, work in single-call mode.
	size_t vli_pos_internal = 0;
	if (vli_pos == NULL)
		vli_pos = &vli_pos_internal;

	// Initialize *vli when starting to decode a new integer.
	if (*vli_pos == 0)
		*vli = 0;

	// Validate the arguments.
	if (*vli_pos >= LZMA_VLI_BYTES_MAX || *in_pos >= in_size
			|| (*vli >> (*vli_pos * 7)) != 0)
		return LZMA_PROG_ERROR;;

	do {
		// Read the next byte.
		*vli |= (lzma_vli)(in[*in_pos] & 0x7F) << (*vli_pos * 7);
		++*vli_pos;

		// Check if this is the last byte of a multibyte integer.
		if (!(in[*in_pos] & 0x80)) {
			// We don't allow using variable-length integers as
			// padding i.e. the encoding must use the most the
			// compact form.
			if (in[(*in_pos)++] == 0x00 && *vli_pos > 1)
				return LZMA_DATA_ERROR;

			return vli_pos == &vli_pos_internal
					? LZMA_OK : LZMA_STREAM_END;
		}

		++*in_pos;

		// There is at least one more byte coming. If we have already
		// read maximum number of bytes, the integer is considered
		// corrupt.
		//
		// If we need bigger integers in future, old versions liblzma
		// will confusingly indicate the file being corrupt istead of
		// unsupported. I suppose it's still better this way, because
		// in the foreseeable future (writing this in 2008) the only
		// reason why files would appear having over 63-bit integers
		// is that the files are simply corrupt.
		if (*vli_pos == LZMA_VLI_BYTES_MAX)
			return LZMA_DATA_ERROR;

	} while (*in_pos < in_size);

	return vli_pos == &vli_pos_internal ? LZMA_DATA_ERROR : LZMA_OK;
}
