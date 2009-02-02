///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_buffer_encoder.c
/// \brief      Single-call raw encoding
//
//  Copyright (C) 2009 Lasse Collin
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

#include "filter_encoder.h"


extern LZMA_API(lzma_ret)
lzma_raw_buffer_encode(const lzma_filter *filters, lzma_allocator *allocator,
		const uint8_t *in, size_t in_size, uint8_t *out,
		size_t *out_pos, size_t out_size)
{
	// Validate what isn't validated later in filter_common.c.
	if ((in == NULL && in_size != 0) || out == NULL
			|| out_pos == NULL || *out_pos > out_size)
		return LZMA_PROG_ERROR;

	// Initialize the encoder
	lzma_next_coder next = LZMA_NEXT_CODER_INIT;
	return_if_error(lzma_raw_encoder_init(&next, allocator, filters));

	// Store the output position so that we can restore it if
	// something goes wrong.
	const size_t out_start = *out_pos;

	// Do the actual encoding and free coder's memory.
	size_t in_pos = 0;
	lzma_ret ret = next.code(next.coder, allocator, in, &in_pos, in_size,
			out, out_pos, out_size, LZMA_FINISH);
	lzma_next_end(&next, allocator);

	if (ret == LZMA_STREAM_END) {
		ret = LZMA_OK;
	} else {
		if (ret == LZMA_OK) {
			// Output buffer was too small.
			assert(*out_pos == out_size);
			ret = LZMA_BUF_ERROR;
		}

		// Restore the output position.
		*out_pos = out_start;
	}

	return ret;
}
