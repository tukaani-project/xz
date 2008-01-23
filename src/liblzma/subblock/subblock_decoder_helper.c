///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_decoder_helper.c
/// \brief      Helper filter for the Subblock decoder
///
/// This filter is used to indicate End of Input for subfilters needing it.
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

#include "subblock_decoder_helper.h"


struct lzma_coder_s {
	const lzma_options_subblock_helper *options;
};


static lzma_ret
helper_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action lzma_attribute((unused)))
{
	// If end_was_reached is true, we cannot have any input.
	assert(!coder->options->end_was_reached || *in_pos == in_size);

	// We can safely copy as much as possible, because we are never
	// given more data than a single Subblock Data field.
	bufcpy(in, in_pos, in_size, out, out_pos, out_size);

	// Return LZMA_STREAM_END when instructed so by the Subblock decoder.
	return coder->options->end_was_reached ? LZMA_STREAM_END : LZMA_OK;
}


static void
helper_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_subblock_decoder_helper_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters)
{
	// This is always the last filter in the chain.
	assert(filters[1].init == NULL);

	// We never know uncompressed size.
	assert(filters[0].uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;
		
		next->code = &helper_decode;
		next->end = &helper_end;
	}

	next->coder->options = filters[0].options;

	return LZMA_OK;
}
