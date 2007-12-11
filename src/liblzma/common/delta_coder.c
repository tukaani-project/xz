///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_coder.c
/// \brief      Encoder and decoder for the Delta filter
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

#include "delta_coder.h"


struct lzma_coder_s {
	/// Next coder in the chain
	lzma_next_coder next;

	/// Uncompressed size - This is needed when we are the last
	/// filter in the chain.
	lzma_vli uncompressed_size;

	/// Delta distance
	size_t distance;

	/// True if we are encoding; false if decoding
	bool is_encoder;

	/// Position in history[]
	uint8_t pos;

	/// Buffer to hold history of the original data
	uint8_t history[LZMA_DELTA_DISTANCE_MAX];
};


static void
encode_buffer(lzma_coder *coder, uint8_t *buffer, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		const uint8_t tmp = coder->history[
				(distance + coder->pos) & 0xFF];
		coder->history[coder->pos--] = buffer[i];
		buffer[i] -= tmp;
	}

	return;
}


static void
decode_buffer(lzma_coder *coder, uint8_t *buffer, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		buffer[i] += coder->history[(distance + coder->pos) & 0xFF];
		coder->history[coder->pos--] = buffer[i];
	}

	return;
}


static lzma_ret
delta_code(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	const size_t out_start = *out_pos;
	size_t size;
	lzma_ret ret;

	if (coder->next.code == NULL) {
		if (!coder->is_encoder) {
			// Limit in_size so that we don't copy too much.
			if ((lzma_vli)(in_size - *in_pos)
					> coder->uncompressed_size)
				in_size = *in_pos + (size_t)(
						coder->uncompressed_size);
		}

		size = bufcpy(in, in_pos, in_size, out, out_pos, out_size);

		if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
			coder->uncompressed_size -= size;

		// action can be LZMA_FINISH only in the encoder.
		ret = (action == LZMA_FINISH && *in_pos == in_size)
					|| coder->uncompressed_size == 0
				? LZMA_STREAM_END : LZMA_OK;

	} else {
		ret = coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);
		if (ret != LZMA_OK && ret != LZMA_STREAM_END)
			return ret;

		size = *out_pos - out_start;
	}

	if (coder->is_encoder)
		encode_buffer(coder, out + out_start, size);
	else
		decode_buffer(coder, out + out_start, size);

	return ret;
}


static void
delta_coder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
delta_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	// Allocate memory for the decoder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &delta_code;
		next->end = &delta_coder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Copy Uncompressed Size which is used to limit the output size.
	next->coder->uncompressed_size = filters[0].uncompressed_size;

	// The coder acts slightly differently as encoder and decoder.
	next->coder->is_encoder = is_encoder;

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


#ifdef HAVE_ENCODER
extern lzma_ret
lzma_delta_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return delta_coder_init(next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER
extern lzma_ret
lzma_delta_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return delta_coder_init(next, allocator, filters, false);
}
#endif
