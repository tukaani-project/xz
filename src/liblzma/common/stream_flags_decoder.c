///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_flags_decoder.c
/// \brief      Decodes Stream Header and tail from .lzma files
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

#include "stream_flags_decoder.h"
#include "stream_common.h"


////////////
// Common //
////////////

struct lzma_coder_s {
	enum {
		SEQ_HEADER_MAGIC,
		SEQ_HEADER_FLAGS,
		SEQ_HEADER_CRC32,

		SEQ_FOOTER_FLAGS,
		SEQ_FOOTER_MAGIC,
	} sequence;

	size_t pos;
	uint32_t crc32;

	lzma_stream_flags *options;
};


static void
stream_header_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_free(coder, allocator);
	return;
}


static bool
stream_flags_decode(const uint8_t *in, lzma_stream_flags *options)
{
	// Reserved bits must be unset.
	if (*in & 0xE0)
		return true;

	options->check = *in & 0x07;
	options->has_crc32 = (*in & 0x08) != 0;
	options->is_multi = (*in & 0x10) != 0;

	return false;
}


////////////
// Header //
////////////

static lzma_ret
stream_header_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_HEADER_MAGIC:
		if (in[*in_pos] != lzma_header_magic[coder->pos])
			return LZMA_DATA_ERROR;

		++*in_pos;

		if (++coder->pos == sizeof(lzma_header_magic)) {
			coder->pos = 0;
			coder->sequence = SEQ_HEADER_FLAGS;
		}

		break;

	case SEQ_HEADER_FLAGS:
		if (stream_flags_decode(in + *in_pos, coder->options))
			return LZMA_HEADER_ERROR;

		coder->crc32 = lzma_crc32(in + *in_pos, 1, 0);

		++*in_pos;
		coder->sequence = SEQ_HEADER_CRC32;
		break;

	case SEQ_HEADER_CRC32:
		if (in[*in_pos] != ((coder->crc32 >> (coder->pos * 8)) & 0xFF))
			return LZMA_DATA_ERROR;

		++*in_pos;

		if (++coder->pos == 4)
			return LZMA_STREAM_END;

		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
stream_header_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;
	}

	// Set the function pointers unconditionally, because they may
	// have been pointing to footer decoder too.
	next->code = &stream_header_decode;
	next->end = &stream_header_decoder_end;

	next->coder->sequence = SEQ_HEADER_MAGIC;
	next->coder->pos = 0;
	next->coder->crc32 = 0;
	next->coder->options = options;

	return LZMA_OK;
}


extern lzma_ret
lzma_stream_header_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options)
{
	lzma_next_coder_init(
			stream_header_decoder_init, next, allocator, options);
}


extern LZMA_API lzma_ret
lzma_stream_header_decoder(lzma_stream *strm, lzma_stream_flags *options)
{
	lzma_next_strm_init(strm, stream_header_decoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}


//////////
// Tail //
//////////

static lzma_ret
stream_tail_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_FOOTER_FLAGS:
		if (stream_flags_decode(in + *in_pos, coder->options))
			return LZMA_HEADER_ERROR;

		++*in_pos;
		coder->sequence = SEQ_FOOTER_MAGIC;
		break;

	case SEQ_FOOTER_MAGIC:
		if (in[*in_pos] != lzma_footer_magic[coder->pos])
			return LZMA_DATA_ERROR;

		++*in_pos;

		if (++coder->pos == sizeof(lzma_footer_magic))
			return LZMA_STREAM_END;

		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
stream_tail_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;
	}

	// Set the function pointers unconditionally, because they may
	// have been pointing to footer decoder too.
	next->code = &stream_tail_decode;
	next->end = &stream_header_decoder_end;

	next->coder->sequence = SEQ_FOOTER_FLAGS;
	next->coder->pos = 0;
	next->coder->options = options;

	return LZMA_OK;
}


extern lzma_ret
lzma_stream_tail_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options)
{
	lzma_next_coder_init2(next, allocator, stream_header_decoder_init,
			stream_tail_decoder_init, allocator, options);
}


extern LZMA_API lzma_ret
lzma_stream_tail_decoder(lzma_stream *strm, lzma_stream_flags *options)
{
	lzma_next_strm_init2(strm, stream_header_decoder_init,
			stream_tail_decoder_init, strm->allocator, options);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
