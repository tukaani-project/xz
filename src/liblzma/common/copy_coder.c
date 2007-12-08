///////////////////////////////////////////////////////////////////////////////
//
/// \file       copy_coder.c
/// \brief      The Copy filter encoder and decoder
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

#include "copy_coder.h"


struct lzma_coder_s {
	lzma_next_coder next;
	lzma_vli uncompressed_size;
	bool is_encoder;
};


static lzma_ret
copy_code(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// If we aren't the last filter in the chain, the Copy filter
	// is totally useless. Note that it is job of the next coder to
	// take care of Uncompressed Size, so we don't need to update our
	// coder->uncompressed_size at all.
	if (coder->next.code != NULL)
		return coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

	// If we get here, we are the last filter in the chain.

	const size_t in_avail = in_size - *in_pos;

	if (coder->is_encoder) {
		// Check that we don't have too much input.
		if ((lzma_vli)(in_avail) > coder->uncompressed_size)
			return LZMA_DATA_ERROR;

		// Check that once LZMA_FINISH has been given, the
		// amount of input matches uncompressed_size if it
		// is known.
		if (action == LZMA_FINISH && coder->uncompressed_size
					!= LZMA_VLI_VALUE_UNKNOWN
				&& coder->uncompressed_size
					!= (lzma_vli)(in_avail))
			return LZMA_DATA_ERROR;

	} else {
		// Limit in_size so that we don't copy too much.
		if ((lzma_vli)(in_avail) > coder->uncompressed_size)
			in_size = *in_pos + (size_t)(coder->uncompressed_size);
	}

	// Store the old input position, which is needed to update
	// coder->uncompressed_size.
	const size_t in_start = *in_pos;

	// We are the last coder in the chain.
	// Just copy as much data as possible.
	bufcpy(in, in_pos, in_size, out, out_pos, out_size);

	// Update uncompressed_size if it is known.
	if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		coder->uncompressed_size -= *in_pos - in_start;

	// action can be LZMA_FINISH only in the encoder.
	if ((action == LZMA_FINISH && *in_pos == in_size)
			|| coder->uncompressed_size == 0)
		return LZMA_STREAM_END;

	return LZMA_OK;
}


static void
copy_coder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
copy_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	// Allocate memory for the decoder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &copy_code;
		next->end = &copy_coder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Copy Uncompressed Size which is used to limit the output size.
	next->coder->uncompressed_size = filters[0].uncompressed_size;

	// The coder acts slightly differently as encoder and decoder.
	next->coder->is_encoder = is_encoder;

	// Initialize the next decoder in the chain, if any.
	return lzma_next_filter_init(
			&next->coder->next, allocator, filters + 1);
}


#ifdef HAVE_ENCODER
extern lzma_ret
lzma_copy_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_next_coder_init(copy_coder_init, next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER
extern lzma_ret
lzma_copy_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_next_coder_init(copy_coder_init, next, allocator, filters, false);
}
#endif
