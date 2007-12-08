///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_header_decoder.c
/// \brief      Decodes Block Header from .lzma files
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

#include "common.h"
#include "check.h"


struct lzma_coder_s {
	lzma_options_block *options;

	enum {
		SEQ_FLAGS_1,
		SEQ_FLAGS_2,
		SEQ_COMPRESSED_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_FILTER_FLAGS_INIT,
		SEQ_FILTER_FLAGS_DECODE,
		SEQ_CRC32,
		SEQ_PADDING
	} sequence;

	/// Position in variable-length integers
	size_t pos;

	/// CRC32 of the Block Header
	uint32_t crc32;

	lzma_next_coder filter_flags_decoder;
};


static bool
update_sequence(lzma_coder *coder)
{
	switch (coder->sequence) {
	case SEQ_FLAGS_2:
		if (coder->options->compressed_size
				!= LZMA_VLI_VALUE_UNKNOWN) {
			coder->pos = 0;
			coder->sequence = SEQ_COMPRESSED_SIZE;
			break;
		}

	// Fall through

	case SEQ_COMPRESSED_SIZE:
		if (coder->options->uncompressed_size
				!= LZMA_VLI_VALUE_UNKNOWN) {
			coder->pos = 0;
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
			break;
		}

	// Fall through

	case SEQ_UNCOMPRESSED_SIZE:
		coder->pos = 0;

	// Fall through

	case SEQ_FILTER_FLAGS_DECODE:
		if (coder->options->filters[coder->pos].id
				!= LZMA_VLI_VALUE_UNKNOWN) {
			coder->sequence = SEQ_FILTER_FLAGS_INIT;
			break;
		}

		if (coder->options->has_crc32) {
			coder->pos = 0;
			coder->sequence = SEQ_CRC32;
			break;
		}

	case SEQ_CRC32:
		if (coder->options->padding != 0) {
			coder->pos = 0;
			coder->sequence = SEQ_PADDING;
			break;
		}

		return true;

	default:
		assert(0);
		return true;
	}

	return false;
}


static lzma_ret
block_header_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_FLAGS_1:
		// Check that the reserved bit is unset. Use HEADER_ERROR
		// because newer version of liblzma may support the reserved
		// bit, although it is likely that this is just a broken file.
		if (in[*in_pos] & 0x40)
			return LZMA_HEADER_ERROR;

		// Number of filters: we prepare appropriate amount of
		// variables for variable-length integer parsing. The
		// initialization function has already reset the rest
		// of the values to LZMA_VLI_VALUE_UNKNOWN, which allows
		// us to later know how many filters there are.
		for (int i = (int)(in[*in_pos] & 0x07) - 1; i >= 0; --i)
			coder->options->filters[i].id = 0;

		// End of Payload Marker flag
		coder->options->has_eopm = (in[*in_pos] & 0x08) != 0;

		// Compressed Size: Prepare for variable-length integer
		// parsing if it is known.
		if (in[*in_pos] & 0x10)
			coder->options->compressed_size = 0;

		// Uncompressed Size: the same.
		if (in[*in_pos] & 0x20)
			coder->options->uncompressed_size = 0;

		// Is Metadata Block flag
		coder->options->is_metadata = (in[*in_pos] & 0x80) != 0;

		// We need at least one: Uncompressed Size or EOPM.
		if (coder->options->uncompressed_size == LZMA_VLI_VALUE_UNKNOWN
				&& !coder->options->has_eopm)
			return LZMA_DATA_ERROR;

		// Update header CRC32.
		coder->crc32 = lzma_crc32(in + *in_pos, 1, coder->crc32);

		++*in_pos;
		coder->sequence = SEQ_FLAGS_2;
		break;

	case SEQ_FLAGS_2:
		// Check that the reserved bits are unset.
		if (in[*in_pos] & 0xE0)
			return LZMA_DATA_ERROR;

		// Get the size of Header Padding.
		coder->options->padding = in[*in_pos] & 0x1F;

		coder->crc32 = lzma_crc32(in + *in_pos, 1, coder->crc32);

		++*in_pos;

		if (update_sequence(coder))
			return LZMA_STREAM_END;

		break;

	case SEQ_COMPRESSED_SIZE: {
		// Store the old input position to be used when
		// updating coder->header_crc32.
		const size_t in_start = *in_pos;

		const lzma_ret ret = lzma_vli_decode(
				&coder->options->compressed_size,
				&coder->pos, in, in_pos, in_size);

		const size_t in_used = *in_pos - in_start;

		coder->options->compressed_reserve += in_used;
		assert(coder->options->compressed_reserve
				<= LZMA_VLI_BYTES_MAX);

		coder->options->header_size += in_used;

		coder->crc32 = lzma_crc32(in + in_start, in_used,
				coder->crc32);

		if (ret != LZMA_STREAM_END)
			return ret;

		if (update_sequence(coder))
			return LZMA_STREAM_END;

		break;
	}

	case SEQ_UNCOMPRESSED_SIZE: {
		const size_t in_start = *in_pos;

		const lzma_ret ret = lzma_vli_decode(
				&coder->options->uncompressed_size,
				&coder->pos, in, in_pos, in_size);

		const size_t in_used = *in_pos - in_start;

		coder->options->uncompressed_reserve += in_used;
		assert(coder->options->uncompressed_reserve
				<= LZMA_VLI_BYTES_MAX);

		coder->options->header_size += in_used;

		coder->crc32 = lzma_crc32(in + in_start, in_used,
				coder->crc32);

		if (ret != LZMA_STREAM_END)
			return ret;

		if (update_sequence(coder))
			return LZMA_STREAM_END;

		break;
	}

	case SEQ_FILTER_FLAGS_INIT: {
		assert(coder->options->filters[coder->pos].id
				!= LZMA_VLI_VALUE_UNKNOWN);

		const lzma_ret ret = lzma_filter_flags_decoder_init(
				&coder->filter_flags_decoder, allocator,
				&coder->options->filters[coder->pos]);
		if (ret != LZMA_OK)
			return ret;

		coder->sequence = SEQ_FILTER_FLAGS_DECODE;
	}

	// Fall through

	case SEQ_FILTER_FLAGS_DECODE: {
		const size_t in_start = *in_pos;

		const lzma_ret ret = coder->filter_flags_decoder.code(
				coder->filter_flags_decoder.coder,
				allocator, in, in_pos, in_size,
				NULL, NULL, 0, LZMA_RUN);

		const size_t in_used = *in_pos - in_start;
		coder->options->header_size += in_used;
		coder->crc32 = lzma_crc32(in + in_start,
				in_used, coder->crc32);

		if (ret != LZMA_STREAM_END)
			return ret;

		++coder->pos;

		if (update_sequence(coder))
			return LZMA_STREAM_END;

		break;
	}

	case SEQ_CRC32:
		assert(coder->options->has_crc32);

		if (in[*in_pos] != ((coder->crc32 >> (coder->pos * 8)) & 0xFF))
			return LZMA_DATA_ERROR;

		++*in_pos;
		++coder->pos;

		// Check if we reached end of the CRC32 field.
		if (coder->pos == 4) {
			coder->options->header_size += 4;

			if (update_sequence(coder))
				return LZMA_STREAM_END;
		}

		break;

	case SEQ_PADDING:
		if (in[*in_pos] != 0x00)
			return LZMA_DATA_ERROR;

		++*in_pos;
		++coder->options->header_size;
		++coder->pos;

		if (coder->pos < (size_t)(coder->options->padding))
			break;

		return LZMA_STREAM_END;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
block_header_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->filter_flags_decoder, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_block_header_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_options_block *options)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_header_decode;
		next->end = &block_header_decoder_end;
		next->coder->filter_flags_decoder = LZMA_NEXT_CODER_INIT;
	}

	// Assume that Compressed Size and Uncompressed Size are unknown.
	options->compressed_size = LZMA_VLI_VALUE_UNKNOWN;
	options->uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;

	// We will calculate the sizes of these fields too so that the
	// application may rewrite the header if it wishes so.
	options->compressed_reserve = 0;
	options->uncompressed_reserve = 0;

	// The Block Flags field is always present, so include its size here
	// and we don't need to worry about it in block_header_decode().
	options->header_size = 2;

	// Reset filters[] to indicate empty list of filters.
	// See SEQ_FLAGS_1 in block_header_decode() for reasoning of this.
	for (size_t i = 0; i < 8; ++i) {
		options->filters[i].id = LZMA_VLI_VALUE_UNKNOWN;
		options->filters[i].options = NULL;
	}

	next->coder->options = options;
	next->coder->sequence = SEQ_FLAGS_1;
	next->coder->pos = 0;
	next->coder->crc32 = 0;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_block_header_decoder(lzma_stream *strm,
		lzma_options_block *options)
{
	lzma_next_strm_init(strm, lzma_block_header_decoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
