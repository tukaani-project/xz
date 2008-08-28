///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_common.c
/// \brief      Common stuff for Delta encoder and decoder
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

#include "delta_common.h"


static void
delta_coder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_delta_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, lzma_code_function code)
{
	// Allocate memory for the decoder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		// End function is the same for encoder and decoder.
		next->end = &delta_coder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Coding function is different for encoder and decoder.
	next->code = code;

	// Set the delta distance.
	if (filters[0].options == NULL)
		return LZMA_PROG_ERROR;
	next->coder->distance = ((lzma_options_delta *)(filters[0].options))
			->distance;
	if (next->coder->distance < LZMA_DELTA_DISTANCE_MIN
			|| next->coder->distance > LZMA_DELTA_DISTANCE_MAX)
		return LZMA_HEADER_ERROR;

	// Initialize the rest of the variables.
	next->coder->pos = 0;
	memzero(next->coder->history, LZMA_DELTA_DISTANCE_MAX);

	// Initialize the next decoder in the chain, if any.
	return lzma_next_filter_init(&next->coder->next,
				allocator, filters + 1);
}
