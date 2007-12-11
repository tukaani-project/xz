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
};


#ifdef HAVE_ENCODER
static lzma_ret
copy_encode(lzma_coder *coder, lzma_allocator *allocator,
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

	// We are the last coder in the chain.
	// Just copy as much data as possible.
	bufcpy(in, in_pos, in_size, out, out_pos, out_size);

	// LZMA_SYNC_FLUSH and LZMA_FINISH are the same thing for us.
	if (action != LZMA_RUN && *in_pos == in_size)
		return LZMA_STREAM_END;

	return LZMA_OK;
}
#endif


#ifdef HAVE_DECODER
static lzma_ret
copy_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	if (coder->next.code != NULL)
		return coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

	assert(coder->uncompressed_size <= LZMA_VLI_VALUE_MAX);

	const size_t in_avail = in_size - *in_pos;

	// Limit in_size so that we don't copy too much.
	if ((lzma_vli)(in_avail) > coder->uncompressed_size)
		in_size = *in_pos + (size_t)(coder->uncompressed_size);

	// We are the last coder in the chain.
	// Just copy as much data as possible.
	const size_t in_used = bufcpy(
			in, in_pos, in_size, out, out_pos, out_size);

	// Update uncompressed_size if it is known.
	if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		coder->uncompressed_size -= in_used;

	return coder->uncompressed_size == 0 ? LZMA_STREAM_END : LZMA_OK;
}
#endif


static void
copy_coder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
copy_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, lzma_code_function encode)
{
	// Allocate memory for the decoder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = encode;
		next->end = &copy_coder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Copy Uncompressed Size which is used to limit the output size.
	next->coder->uncompressed_size = filters[0].uncompressed_size;

	// Initialize the next decoder in the chain, if any.
	return lzma_next_filter_init(
			&next->coder->next, allocator, filters + 1);
}


#ifdef HAVE_ENCODER
extern lzma_ret
lzma_copy_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_next_coder_init(copy_coder_init, next, allocator, filters,
			&copy_encode);
}
#endif


#ifdef HAVE_DECODER
extern lzma_ret
lzma_copy_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_next_coder_init(copy_coder_init, next, allocator, filters,
			&copy_decode);
}
#endif
