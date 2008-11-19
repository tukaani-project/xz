///////////////////////////////////////////////////////////////////////////////
//
/// \file       index_encoder.c
/// \brief      Encodes the Index field
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

#include "index_encoder.h"
#include "index.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_INDICATOR,
		SEQ_COUNT,
		SEQ_UNPADDED,
		SEQ_UNCOMPRESSED,
		SEQ_NEXT,
		SEQ_PADDING,
		SEQ_CRC32,
	} sequence;

	/// Index given to us to encode. Note that we modify it in sense that
	/// we read it, and read position is tracked in lzma_index structure.
	lzma_index *index;

	/// The current Index Record being encoded
	lzma_index_record record;

	/// Position in integers
	size_t pos;

	/// CRC32 of the List of Records field
	uint32_t crc32;
};


static lzma_ret
index_encode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in lzma_attribute((unused)),
		size_t *restrict in_pos lzma_attribute((unused)),
		size_t in_size lzma_attribute((unused)),
		uint8_t *restrict out, size_t *restrict out_pos,
		size_t out_size, lzma_action action lzma_attribute((unused)))
{
	// Position where to start calculating CRC32. The idea is that we
	// need to call lzma_crc32() only once per call to index_encode().
	const size_t out_start = *out_pos;

	// Return value to use if we return at the end of this function.
	// We use "goto out" to jump out of the while-switch construct
	// instead of returning directly, because that way we don't need
	// to copypaste the lzma_crc32() call to many places.
	lzma_ret ret = LZMA_OK;

	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_INDICATOR:
		out[*out_pos] = 0x00;
		++*out_pos;
		coder->sequence = SEQ_COUNT;
		break;

	case SEQ_COUNT: {
		const lzma_vli index_count = lzma_index_count(coder->index);
		ret = lzma_vli_encode(index_count, &coder->pos,
				out, out_pos, out_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;
		coder->sequence = SEQ_NEXT;
		break;
	}

	case SEQ_NEXT:
		if (lzma_index_read(coder->index, &coder->record)) {
			// Get the size of the Index Padding field.
			coder->pos = lzma_index_padding_size(coder->index);
			assert(coder->pos <= 3);
			coder->sequence = SEQ_PADDING;
			break;
		}

		// Unpadded Size must be within valid limits.
		if (coder->record.unpadded_size < UNPADDED_SIZE_MIN
				|| coder->record.unpadded_size
					> UNPADDED_SIZE_MAX)
			return LZMA_PROG_ERROR;

		coder->sequence = SEQ_UNPADDED;

	// Fall through

	case SEQ_UNPADDED:
	case SEQ_UNCOMPRESSED: {
		const lzma_vli size = coder->sequence == SEQ_UNPADDED
				? coder->record.unpadded_size
				: coder->record.uncompressed_size;

		ret = lzma_vli_encode(size, &coder->pos,
				out, out_pos, out_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;

		// Advance to SEQ_UNCOMPRESSED or SEQ_NEXT.
		++coder->sequence;
		break;
	}

	case SEQ_PADDING:
		if (coder->pos > 0) {
			--coder->pos;
			out[(*out_pos)++] = 0x00;
			break;
		}

		// Finish the CRC32 calculation.
		coder->crc32 = lzma_crc32(out + out_start,
				*out_pos - out_start, coder->crc32);

		coder->sequence = SEQ_CRC32;

	// Fall through

	case SEQ_CRC32:
		// We don't use the main loop, because we don't want
		// coder->crc32 to be touched anymore.
		do {
			if (*out_pos == out_size)
				return LZMA_OK;

			out[*out_pos] = (coder->crc32 >> (coder->pos * 8))
					& 0xFF;
			++*out_pos;

		} while (++coder->pos < 4);

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

out:
	// Update the CRC32.
	coder->crc32 = lzma_crc32(out + out_start,
			*out_pos - out_start, coder->crc32);

	return ret;
}


static void
index_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_index_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_index *i)
{
	lzma_next_coder_init(lzma_index_encoder_init, next, allocator);

	if (i == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &index_encode;
		next->end = &index_encoder_end;
	}

	lzma_index_rewind(i);

	next->coder->sequence = SEQ_INDICATOR;
	next->coder->index = i;
	next->coder->pos = 0;
	next->coder->crc32 = 0;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_index_encoder(lzma_stream *strm, lzma_index *i)
{
	lzma_next_strm_init(lzma_index_encoder_init, strm, i);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
