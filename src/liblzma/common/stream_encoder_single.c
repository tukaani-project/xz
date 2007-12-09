///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_encoder_single.c
/// \brief      Encodes Single-Block .lzma files
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

#include "stream_common.h"
#include "block_encoder.h"


struct lzma_coder_s {
	/// Uncompressed Size, Backward Size, and Footer Magic Bytes are
	/// part of Block in the file format specification, but it is simpler
	/// to implement them as part of Stream.
	enum {
		SEQ_HEADERS,
		SEQ_DATA,
		SEQ_FOOTER,
	} sequence;

	/// Block encoder
	lzma_next_coder block_encoder;

	/// Block encoder options
	lzma_options_block block_options;

	/// Stream Flags; we need to have these in this struct so that we
	/// can encode Stream Footer.
	lzma_stream_flags stream_flags;

	/// Stream Header + Block Header, or Stream Footer
	uint8_t *header;
	size_t header_pos;
	size_t header_size;
};


static lzma_ret
stream_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out, size_t *out_pos,
		size_t out_size, lzma_action action)
{
	// NOTE: We don't check if the amount of input is in the proper limits,
	// because the Block encoder will do it for us.

	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_HEADERS:
		bufcpy(coder->header, &coder->header_pos, coder->header_size,
				out, out_pos, out_size);

		if (coder->header_pos == coder->header_size) {
			coder->header_pos = 0;
			coder->sequence = SEQ_DATA;
		}

		break;

	case SEQ_DATA: {
		const lzma_ret ret = coder->block_encoder.code(
				coder->block_encoder.coder, allocator,
				in, in_pos, in_size,
				out, out_pos, out_size, action);
		if (ret != LZMA_STREAM_END || action == LZMA_SYNC_FLUSH)
			return ret;

		assert(*in_pos == in_size);

		assert(coder->header_size >= LZMA_STREAM_TAIL_SIZE);
		coder->header_size = LZMA_STREAM_TAIL_SIZE;

		return_if_error(lzma_stream_tail_encode(
				coder->header, &coder->stream_flags));

		coder->sequence = SEQ_FOOTER;
		break;
	}

	case SEQ_FOOTER:
		bufcpy(coder->header, &coder->header_pos, coder->header_size,
				out, out_pos, out_size);

		return coder->header_pos == coder->header_size
				? LZMA_STREAM_END : LZMA_OK;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
stream_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->block_encoder, allocator);
	lzma_free(coder->header, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
stream_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_options_stream *options)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &stream_encode;
		next->end = &stream_encoder_end;
		next->coder->block_encoder = LZMA_NEXT_CODER_INIT;
	} else {
		// Free the previous buffer, if any.
		lzma_free(next->coder->header, allocator);
	}

	// At this point, next->coder->header points to nothing useful.
	next->coder->header = NULL;

	// Basic initializations
	next->coder->sequence = SEQ_HEADERS;
	next->coder->header_pos = 0;

	// Initialize next->coder->stream_flags.
	next->coder->stream_flags = (lzma_stream_flags){
		.check = options->check,
		.has_crc32 = options->has_crc32,
		.is_multi = false,
	};

	// Initialize next->coder->block_options.
	next->coder->block_options = (lzma_options_block){
		.check = options->check,
		.has_crc32 = options->has_crc32,
		.has_eopm = options->uncompressed_size
				== LZMA_VLI_VALUE_UNKNOWN,
		.is_metadata = false,
		.has_uncompressed_size_in_footer = options->uncompressed_size
				== LZMA_VLI_VALUE_UNKNOWN,
		.has_backward_size = true,
		.handle_padding = false,
		.compressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = options->uncompressed_size,
		.compressed_reserve = 0,
		.uncompressed_reserve = 0,
		.total_size = LZMA_VLI_VALUE_UNKNOWN,
		.total_limit = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_limit = LZMA_VLI_VALUE_UNKNOWN,
		.padding = LZMA_BLOCK_HEADER_PADDING_AUTO,
		.alignment = options->alignment + LZMA_STREAM_HEADER_SIZE,
	};
	memcpy(next->coder->block_options.filters, options->filters,
				sizeof(options->filters));

	return_if_error(lzma_block_header_size(&next->coder->block_options));

	// Encode Stream Flags and Block Header into next->coder->header.
	next->coder->header_size = (size_t)(LZMA_STREAM_HEADER_SIZE)
			+ next->coder->block_options.header_size;
	next->coder->header = lzma_alloc(next->coder->header_size, allocator);
	if (next->coder->header == NULL)
		return LZMA_MEM_ERROR;

	return_if_error(lzma_stream_header_encode(next->coder->header,
			&next->coder->stream_flags));

	return_if_error(lzma_block_header_encode(
			next->coder->header + LZMA_STREAM_HEADER_SIZE,
			&next->coder->block_options));

	// Initialize the Block encoder.
	return lzma_block_encoder_init(&next->coder->block_encoder, allocator,
			&next->coder->block_options);
}


/*
extern lzma_ret
lzma_stream_encoder_single_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_options_stream *options)
{
	lzma_next_coder_init(stream_encoder_init, allocator, options);
}
*/


extern LZMA_API lzma_ret
lzma_stream_encoder_single(
		lzma_stream *strm, const lzma_options_stream *options)
{
	lzma_next_strm_init(strm, stream_encoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
