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
#include "lz_decoder.h"


struct lzma_coder_s {
	lzma_next_coder next;

	enum {
		SEQ_PROPERTIES,
		SEQ_DICTIONARY_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_CODER_INIT,
		SEQ_CODE,
	} sequence;

	/// Position in the header fields
	size_t pos;

	/// Uncompressed size decoded from the header
	lzma_vli uncompressed_size;

	/// Memory usage limit
	uint64_t memlimit;

	/// Options decoded from the header needed to initialize
	/// the LZMA decoder
	lzma_options_lzma options;
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
		if (lzma_lzma_lclppb_decode(&coder->options, in[*in_pos]))
			return LZMA_FORMAT_ERROR;

		coder->sequence = SEQ_DICTIONARY_SIZE;
		++*in_pos;
		break;

	case SEQ_DICTIONARY_SIZE:
		coder->options.dict_size
				|= (size_t)(in[*in_pos]) << (coder->pos * 8);

		if (++coder->pos == 4) {
			if (coder->options.dict_size > (UINT32_C(1) << 30))
				return LZMA_FORMAT_ERROR;

			// A hack to ditch tons of false positives: We allow
			// only dictionary sizes that are 2^n or 2^n + 2^(n-1).
			// LZMA_Alone created only files with 2^n, but accepts
			// any dictionary size. If someone complains, this
			// will be reconsidered.
			uint32_t d = coder->options.dict_size - 1;
			d |= d >> 2;
			d |= d >> 3;
			d |= d >> 4;
			d |= d >> 8;
			d |= d >> 16;
			++d;

			if (d != coder->options.dict_size)
				return LZMA_FORMAT_ERROR;

			coder->pos = 0;
			coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		}

		++*in_pos;
		break;

	case SEQ_UNCOMPRESSED_SIZE:
		coder->uncompressed_size
				|= (lzma_vli)(in[*in_pos]) << (coder->pos * 8);

		if (++coder->pos == 8) {
			// Another hack to ditch false positives: Assume that
			// if the uncompressed size is known, it must be less
			// than 256 GiB. Again, if someone complains, this
			// will be reconsidered.
			if (coder->uncompressed_size != LZMA_VLI_UNKNOWN
					&& coder->uncompressed_size
						>= (LZMA_VLI_C(1) << 38))
				return LZMA_FORMAT_ERROR;

			coder->pos = 0;
			coder->sequence = SEQ_CODER_INIT;
		}

		++*in_pos;
		break;

	case SEQ_CODER_INIT: {
		// FIXME It is unfair that this doesn't add a fixed amount
		// like lzma_memusage_common() does.
		const uint64_t memusage
				= lzma_lzma_decoder_memusage(&coder->options);

		// Use LZMA_PROG_ERROR since LZMA_Alone decoder cannot be
		// built without LZMA support.
		// FIXME TODO Make the above comment true.
		if (memusage == UINT64_MAX)
			return LZMA_PROG_ERROR;

		if (memusage > coder->memlimit)
			return LZMA_MEMLIMIT_ERROR;

		lzma_filter_info filters[2] = {
			{
				.init = &lzma_lzma_decoder_init,
				.options = &coder->options,
			}, {
				.init = NULL,
			}
		};

		const lzma_ret ret = lzma_next_filter_init(&coder->next,
				allocator, filters);
		if (ret != LZMA_OK)
			return ret;

		// Use a hack to set the uncompressed size.
		lzma_lz_decoder_uncompressed(coder->next.coder,
				coder->uncompressed_size);

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
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_alone_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		uint64_t memlimit)
{
	lzma_next_coder_init(lzma_alone_decoder_init, next, allocator);

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
	next->coder->options.dict_size = 0;
	next->coder->uncompressed_size = 0;
	next->coder->memlimit = memlimit;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_alone_decoder(lzma_stream *strm, uint64_t memlimit)
{
	lzma_next_strm_init(lzma_alone_decoder_init, strm, memlimit);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
