///////////////////////////////////////////////////////////////////////////////
//
/// \file       alone_decoder.c
/// \brief      Decoder for LZMA_Alone files
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

#include "alone_decoder.h"
#include "lzma_decoder.h"


struct lzma_coder_s {
	lzma_next_coder next;

	enum {
		SEQ_PROPERTIES,
		SEQ_DICTIONARY_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_CODER_INIT,
		SEQ_CODE,
	} sequence;

	size_t pos;

	lzma_options_alone options;
};


static lzma_ret
alone_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action)
{
	while (*out_pos < out_size
			&& (coder->sequence == SEQ_CODE || *in_pos < in_size))
	switch (coder->sequence) {
	case SEQ_PROPERTIES:
		if (lzma_lzma_decode_properties(
				&coder->options.lzma, in[*in_pos]))
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_DICTIONARY_SIZE;
		++*in_pos;
		break;

	case SEQ_DICTIONARY_SIZE:
		coder->options.lzma.dictionary_size
				|= (size_t)(in[*in_pos]) << (coder->pos * 8);

		if (++coder->pos == 4) {
			// A hack to ditch tons of false positives: We allow
			// only dictionary sizes that are a power of two.
			// LZMA_Alone didn't create other kinds of files,
			// although it's not impossible that files with
			// other dictionary sizes exist. Well, if someone
			// complains, this will be reconsidered.
			size_t count = 0;
			for (size_t i = 0; i < 32; ++i)
				if (coder->options.lzma.dictionary_size
						& (UINT32_C(1) << i))
					++count;

			if (count != 1 || coder->options.lzma.dictionary_size
					> LZMA_DICTIONARY_SIZE_MAX)
				return LZMA_DATA_ERROR;

			coder->pos = 0;
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		}

		++*in_pos;
		break;

	case SEQ_UNCOMPRESSED_SIZE:
		coder->options.uncompressed_size
				|= (lzma_vli)(in[*in_pos]) << (coder->pos * 8);

		if (++coder->pos == 8) {
			// Another hack to ditch false positives: Assume that
			// if the uncompressed size is known, it must be less
			// than 256 GiB. Again, if someone complains, this
			// will be reconsidered.
			if (coder->options.uncompressed_size
						!= LZMA_VLI_VALUE_UNKNOWN
					&& coder->options.uncompressed_size
						>= (LZMA_VLI_C(1) << 38))
				return LZMA_DATA_ERROR;

			coder->pos = 0;
			coder->sequence = SEQ_CODER_INIT;
		}

		++*in_pos;
		break;

	case SEQ_CODER_INIT: {
		// Two is enough because there won't be implicit filters.
		lzma_filter_info filters[2] = {
			{
				.init = &lzma_lzma_decoder_init,
				.options = &coder->options.lzma,
				.uncompressed_size = coder->options
						.uncompressed_size,
			}, {
				.init = NULL,
			}
		};

		const lzma_ret ret = lzma_next_filter_init(&coder->next,
				allocator, filters);
		if (ret != LZMA_OK)
			return ret;

		coder->sequence = SEQ_CODE;
	}

	// Fall through

	case SEQ_CODE: {
		return coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
alone_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
alone_decoder_init(lzma_next_coder *next, lzma_allocator *allocator)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &alone_decode;
		next->end = &alone_decoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	next->coder->sequence = SEQ_PROPERTIES;
	next->coder->pos = 0;
	next->coder->options.lzma.dictionary_size = 0;
	next->coder->options.uncompressed_size = 0;

	return LZMA_OK;
}


extern lzma_ret
lzma_alone_decoder_init(lzma_next_coder *next, lzma_allocator *allocator)
{
	// We need to use _init2 because we don't pass any varadic args.
	lzma_next_coder_init2(next, allocator, alone_decoder_init,
			alone_decoder_init, allocator);
}


extern LZMA_API lzma_ret
lzma_alone_decoder(lzma_stream *strm)
{
	lzma_next_strm_init2(strm, alone_decoder_init,
			alone_decoder_init, strm->allocator);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
