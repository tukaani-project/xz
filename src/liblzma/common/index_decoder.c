///////////////////////////////////////////////////////////////////////////////
//
/// \file       index_decoder.c
/// \brief      Decodes the Index field
//
//  Copyright (C) 2008 Lasse Collin
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

#include "index.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_INDICATOR,
		SEQ_COUNT,
		SEQ_TOTAL,
		SEQ_UNCOMPRESSED,
		SEQ_PADDING_INIT,
		SEQ_PADDING,
		SEQ_CRC32,
	} sequence;

	/// Target Index
	lzma_index *index;

	/// Number of Records left to decode.
	lzma_vli count;

	/// The most recent Total Size field
	lzma_vli total_size;

	/// The most recent Uncompressed Size field
	lzma_vli uncompressed_size;

	/// Position in integers
	size_t pos;

	/// CRC32 of the List of Records field
	uint32_t crc32;
};


static lzma_ret
index_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	// Similar optimization as in index_encoder.c
	const size_t in_start = *in_pos;
	lzma_ret ret = LZMA_OK;

	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_INDICATOR:
		// Return LZMA_DATA_ERROR instead of e.g. LZMA_PROG_ERROR or
		// LZMA_FORMAT_ERROR, because a typical usage case for Index
		// decoder is when parsing the Stream backwards. If seeking
		// backward from the Stream Footer gives us something that
		// doesn't begin with Index Indicator, the file is considered
		// corrupt, not "programming error" or "unrecognized file
		// format". One could argue that the application should
		// verify the Index Indicator before trying to decode the
		// Index, but well, I suppose it is simpler this way.
		if (in[(*in_pos)++] != 0x00)
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_COUNT;
		break;

	case SEQ_COUNT: {
		ret = lzma_vli_decode(&coder->count, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;
		coder->sequence = coder->count == 0
				? SEQ_PADDING_INIT : SEQ_TOTAL;
		break;
	}

	case SEQ_TOTAL:
	case SEQ_UNCOMPRESSED: {
		lzma_vli *size = coder->sequence == SEQ_TOTAL
				? &coder->total_size
				: &coder->uncompressed_size;

		ret = lzma_vli_decode(size, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;

		if (coder->sequence == SEQ_TOTAL) {
			// Validate that encoded Total Size isn't too big.
			if (coder->total_size > TOTAL_SIZE_ENCODED_MAX)
				return LZMA_DATA_ERROR;

			// Convert the encoded Total Size to the real
			// Total Size.
			coder->total_size = total_size_decode(
					coder->total_size);
			coder->sequence = SEQ_UNCOMPRESSED;
		} else {
			// Add the decoded Record to the Index.
			return_if_error(lzma_index_append(
					coder->index, allocator,
					coder->total_size,
					coder->uncompressed_size));

			// Check if this was the last Record.
			coder->sequence = --coder->count == 0
					? SEQ_PADDING_INIT
					: SEQ_TOTAL;
		}

		break;
	}

	case SEQ_PADDING_INIT:
		coder->pos = lzma_index_padding_size(coder->index);
		coder->sequence = SEQ_PADDING;

	// Fall through

	case SEQ_PADDING:
		if (coder->pos > 0) {
			--coder->pos;
			if (in[(*in_pos)++] != 0x00)
				return LZMA_DATA_ERROR;

			break;
		}

		// Finish the CRC32 calculation.
		coder->crc32 = lzma_crc32(in + in_start,
				*in_pos - in_start, coder->crc32);

		coder->sequence = SEQ_CRC32;

	// Fall through

	case SEQ_CRC32:
		do {
			if (*in_pos == in_size)
				return LZMA_OK;

			if (((coder->crc32 >> (coder->pos * 8)) & 0xFF)
					!= in[(*in_pos)++])
				return LZMA_DATA_ERROR;

		} while (++coder->pos < 4);

		// Make index NULL so we don't free it unintentionally.
		coder->index = NULL;

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

out:
	// Update the CRC32,
	coder->crc32 = lzma_crc32(in + in_start,
			*in_pos - in_start, coder->crc32);

	return ret;
}


static void
index_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_index_end(coder->index, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
index_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_index **i)
{
	if (i == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &index_decode;
		next->end = &index_decoder_end;
		next->coder->index = NULL;
	} else {
		lzma_index_end(next->coder->index, allocator);
	}

	// We always allocate a new lzma_index.
	*i = lzma_index_init(NULL, allocator);
	if (*i == NULL)
		return LZMA_MEM_ERROR;

	// Initialize the rest.
	next->coder->sequence = SEQ_INDICATOR;
	next->coder->index = *i;
	next->coder->pos = 0;
	next->coder->crc32 = 0;

	return LZMA_OK;
}


/*
extern lzma_ret
lzma_index_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_index **i)
{
	lzma_next_coder_init(index_decoder_init, next, allocator, i);
}
*/


extern LZMA_API lzma_ret
lzma_index_decoder(lzma_stream *strm, lzma_index **i)
{
	lzma_next_strm_init(strm, index_decoder_init, i);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
