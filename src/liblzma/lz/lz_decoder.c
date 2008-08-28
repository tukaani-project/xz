///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_decoder.c
/// \brief      LZ out window
//
//  Copyright (C) 1999-2006 Igor Pavlov
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

// liblzma supports multiple LZ77-based filters. The LZ part is shared
// between these filters. The LZ code takes care of dictionary handling
// and passing the data between filters in the chain. The filter-specific
// part decodes from the input buffer to the dictionary.


#include "lz_decoder.h"


struct lzma_coder_s {
	/// Dictionary (history buffer)
	lzma_dict dict;

	/// The actual LZ-based decoder e.g. LZMA
	lzma_lz_decoder lz;

	/// Next filter in the chain, if any. Note that LZMA and LZMA2 are
	/// only allowed as the last filter, but the long-range filter in
	/// future can be in the middle of the chain.
	lzma_next_coder next;

	/// True if the next filter in the chain has returned LZMA_STREAM_END.
	bool next_finished;

	/// True if the LZ decoder (e.g. LZMA) has detected end of payload
	/// marker. This may become true before next_finished becomes true.
	bool this_finished;

        /// Temporary buffer needed when the LZ-based filter is not the last
        /// filter in the chain. The output of the next filter is first
        /// decoded into buffer[], which is then used as input for the actual
        /// LZ-based decoder.
        struct {
                size_t pos;
                size_t size;
                uint8_t buffer[LZMA_BUFFER_SIZE];
        } temp;
};


static lzma_ret
decode_buffer(lzma_coder *coder,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size)
{
	while (true) {
		// Wrap the dictionary if needed.
		if (coder->dict.pos == coder->dict.size)
			coder->dict.pos = 0;

		// Store the current dictionary position. It is needed to know
		// where to start copying to the out[] buffer.
		const size_t dict_start = coder->dict.pos;

		// Calculate how much we allow the process() function to
		// decode. It must not decode past the end of the dictionary
		// buffer, and we don't want it to decode more than is
		// actually needed to fill the out[] buffer.
		coder->dict.limit = coder->dict.pos + MIN(out_size - *out_pos,
				coder->dict.size - coder->dict.pos);

		// Call the process() function to do the actual decoding.
		const lzma_ret ret = coder->lz.code(
				coder->lz.coder, &coder->dict,
				in, in_pos, in_size);

		// Copy the decoded data from the dictionary to the out[]
		// buffer.
		const size_t copy_size = coder->dict.pos - dict_start;
		assert(copy_size <= out_size - *out_pos);
		memcpy(out + *out_pos, coder->dict.buf + dict_start,
				copy_size);
		*out_pos += copy_size;

		// Return if everything got decoded or an error occurred, or
		// if there's no more data to decode.
		if (ret != LZMA_OK || *out_pos == out_size
				|| coder->dict.pos < coder->dict.size)
			return ret;
	}
}


static lzma_ret
lz_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action)
{
	if (coder->next.code == NULL)
		return decode_buffer(coder, in, in_pos, in_size,
				out, out_pos, out_size);

	// We aren't the last coder in the chain, we need to decode
	// our input to a temporary buffer.
	while (*out_pos < out_size) {
		// Fill the temporary buffer if it is empty.
		if (!coder->next_finished
				&& coder->temp.pos == coder->temp.size) {
			coder->temp.pos = 0;
			coder->temp.size = 0;

			const lzma_ret ret = coder->next.code(
					coder->next.coder,
					allocator, in, in_pos, in_size,
					coder->temp.buffer, &coder->temp.size,
					LZMA_BUFFER_SIZE, action);

			if (ret == LZMA_STREAM_END)
				coder->next_finished = true;
			else if (ret != LZMA_OK || coder->temp.size == 0)
				return ret;
		}

		if (coder->this_finished) {
			if (coder->temp.size != 0)
				return LZMA_DATA_ERROR;

			if (coder->next_finished)
				return LZMA_STREAM_END;

			return LZMA_OK;
		}

		const lzma_ret ret = decode_buffer(coder, coder->temp.buffer,
				&coder->temp.pos, coder->temp.size,
				out, out_pos, out_size);

		if (ret == LZMA_STREAM_END)
			coder->this_finished = true;
		else if (ret != LZMA_OK)
			return ret;
		else if (coder->next_finished && *out_pos < out_size)
			return LZMA_DATA_ERROR;
	}

	return LZMA_OK;
}


static void
lz_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder->dict.buf, allocator);

	if (coder->lz.end != NULL)
		coder->lz.end(coder->lz.coder, allocator);
	else
		lzma_free(coder->lz.coder, allocator);

	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_lz_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters,
		lzma_ret (*lz_init)(lzma_lz_decoder *lz,
			lzma_allocator *allocator, const void *options,
			size_t *dict_size))
{
	// Allocate the base structure if it isn't already allocated.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &lz_decode;
		next->end = &lz_decoder_end;

		next->coder->dict.buf = NULL;
		next->coder->dict.size = 0;
		next->coder->lz = LZMA_LZ_DECODER_INIT;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Allocate and initialize the LZ-based decoder. It will also give
	// us the dictionary size.
	size_t dict_size;
	return_if_error(lz_init(&next->coder->lz, allocator,
			filters[0].options, &dict_size));

	// If the dictionary size is very small, increase it to 4096 bytes.
	// This is to prevent constant wrapping of the dictionary, which
	// would slow things down. The downside is that since we don't check
	// separately for the real dictionary size, we may happily accept
	// corrupt files.
	if (dict_size < 4096)
		dict_size = 4096;

	// Make dictionary size a multipe of 16. Some LZ-based decoders like
	// LZMA use the lowest bits lzma_dict.pos to know the alignment of the
	// data. Aligned buffer is also good when memcpying from the
	// dictionary to the output buffer, since applications are
	// recommended to give aligned buffers to liblzma.
	//
	// Avoid integer overflow. FIXME Should the return value be
	// LZMA_HEADER_ERROR or LZMA_MEM_ERROR?
	if (dict_size > SIZE_MAX - 15)
		return LZMA_MEM_ERROR;

	dict_size = (dict_size + 15) & (SIZE_MAX - 15);

	// Allocate and initialize the dictionary.
	if (next->coder->dict.size != dict_size) {
		lzma_free(next->coder->dict.buf, allocator);
		next->coder->dict.buf = lzma_alloc(dict_size, allocator);
		if (next->coder->dict.buf == NULL)
			return LZMA_MEM_ERROR;

		next->coder->dict.size = dict_size;
	}

	dict_reset(&next->coder->dict);

	// Miscellaneous initializations
	next->coder->next_finished = false;
	next->coder->this_finished = false;
	next->coder->temp.pos = 0;
	next->coder->temp.size = 0;

	// Initialize the next filter in the chain, if any.
	return lzma_next_filter_init(&next->coder->next, allocator,
			filters + 1);
}


extern uint64_t
lzma_lz_decoder_memusage(size_t dictionary_size)
{
	return sizeof(lzma_coder) + (uint64_t)(dictionary_size);
}


extern void
lzma_lz_decoder_uncompressed(lzma_coder *coder, lzma_vli uncompressed_size)
{
	coder->lz.set_uncompressed(coder->lz.coder, uncompressed_size);
}
