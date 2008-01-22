///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_multi.c
/// \brief      Easy Multi-Block Stream encoder initialization
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

#include "easy_common.h"
#include "stream_encoder_multi.h"


struct lzma_coder_s {
	lzma_next_coder encoder;
	lzma_options_stream options;
};


static lzma_ret
easy_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	return coder->encoder.code(coder->encoder.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size, action);
}


static void
easy_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->encoder, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
easy_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_easy_level level, lzma_easy_level metadata_level,
		const lzma_extra *header, const lzma_extra *footer)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &easy_encode;
		next->end = &easy_encoder_end;

		next->coder->encoder = LZMA_NEXT_CODER_INIT;
	}

	next->coder->options = (lzma_options_stream){
		.check = LZMA_CHECK_CRC32,
		.has_crc32 = true,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.alignment = 0,
		.header = header,
		.footer = footer,
	};

	if (lzma_easy_set_filters(next->coder->options.filters, level)
			|| lzma_easy_set_filters(
				next->coder->options.metadata_filters,
				metadata_level))
		return LZMA_HEADER_ERROR;

	return lzma_stream_encoder_multi_init(&next->coder->encoder,
			allocator, &next->coder->options);
}


extern LZMA_API lzma_ret
lzma_easy_encoder_multi(lzma_stream *strm,
		lzma_easy_level level, lzma_easy_level metadata_level,
		const lzma_extra *header, const lzma_extra *footer)
{
	// This is more complicated than lzma_easy_encoder_single(),
	// because lzma_stream_encoder_multi() wants that the options
	// structure is available until the encoding is finished.
	lzma_next_strm_init(strm, easy_encoder_init,
			level, metadata_level, header, footer);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FULL_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
