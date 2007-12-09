///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_encoder_multi.c
/// \brief      Encodes Multi-Block .lzma files
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
#include "metadata_encoder.h"


struct lzma_coder_s {
	enum {
		SEQ_STREAM_HEADER_COPY,
		SEQ_HEADER_METADATA_INIT,
		SEQ_HEADER_METADATA_COPY,
		SEQ_HEADER_METADATA_CODE,
		SEQ_DATA_INIT,
		SEQ_DATA_COPY,
		SEQ_DATA_CODE,
		SEQ_FOOTER_METADATA_INIT,
		SEQ_FOOTER_METADATA_COPY,
		SEQ_FOOTER_METADATA_CODE,
		SEQ_STREAM_FOOTER_INIT,
		SEQ_STREAM_FOOTER_COPY,
	} sequence;

	/// Block or Metadata encoder
	lzma_next_coder next;

	/// Options for the Block encoder
	lzma_options_block block_options;

	/// Information about the Stream
	lzma_info *info;

	/// Information about the current Data Block
	lzma_info_iter iter;

	/// Pointer to user-supplied options structure. We don't write to
	/// it, only read instructions from the application, thus this is
	/// const even though the user-supplied pointer from
	/// lzma_options_filter structure isn't.
	const lzma_options_stream *stream_options;

	/// Stream Header or Stream Footer in encoded form
	uint8_t *header;
	size_t header_pos;
	size_t header_size;
};


typedef enum {
	BLOCK_HEADER_METADATA,
	BLOCK_DATA,
	BLOCK_FOOTER_METADATA,
} block_type;


static lzma_ret
block_header_encode(lzma_coder *coder, lzma_allocator *allocator,
		lzma_vli uncompressed_size, block_type type)
{
	assert(coder->header == NULL);

	coder->block_options = (lzma_options_block){
		.check = coder->stream_options->check,
		.has_crc32 = coder->stream_options->has_crc32,
		.has_eopm = true,
		.is_metadata = type != BLOCK_DATA,
		.has_uncompressed_size_in_footer = false,
		.has_backward_size = type == BLOCK_FOOTER_METADATA,
		.handle_padding = false,
		.total_size = LZMA_VLI_VALUE_UNKNOWN,
		.compressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = uncompressed_size,
		.compressed_reserve = 0,
		.uncompressed_reserve = 0,
		.total_limit = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_limit = LZMA_VLI_VALUE_UNKNOWN,
		.padding = LZMA_BLOCK_HEADER_PADDING_AUTO,
	};

	if (type == BLOCK_DATA) {
		memcpy(coder->block_options.filters,
				coder->stream_options->filters,
				sizeof(coder->stream_options->filters));
		coder->block_options.alignment = coder->iter.stream_offset;
	} else {
		memcpy(coder->block_options.filters,
				coder->stream_options->metadata_filters,
				sizeof(coder->stream_options->filters));
		coder->block_options.alignment
				= lzma_info_metadata_alignment_get(
				coder->info, type == BLOCK_HEADER_METADATA);
	}

	return_if_error(lzma_block_header_size(&coder->block_options));

	coder->header_size = coder->block_options.header_size;
	coder->header = lzma_alloc(coder->header_size, allocator);
	if (coder->header == NULL)
		return LZMA_MEM_ERROR;

	return_if_error(lzma_block_header_encode(
			coder->header, &coder->block_options));

	coder->header_pos = 0;
	return LZMA_OK;
}


static lzma_ret
metadata_encoder_init(lzma_coder *coder, lzma_allocator *allocator,
		lzma_metadata *metadata, block_type type)
{
	return_if_error(lzma_info_metadata_set(coder->info, allocator,
			metadata, type == BLOCK_HEADER_METADATA, false));

	const lzma_vli metadata_size = lzma_metadata_size(metadata);
	if (metadata_size == 0)
		return LZMA_PROG_ERROR;

	return_if_error(block_header_encode(
			coder, allocator, metadata_size, type));

	return lzma_metadata_encoder_init(&coder->next, allocator,
			&coder->block_options, metadata);
}


static lzma_ret
data_encoder_init(lzma_coder *coder, lzma_allocator *allocator)
{
	return_if_error(lzma_info_iter_next(&coder->iter, allocator));

	return_if_error(block_header_encode(coder, allocator,
			LZMA_VLI_VALUE_UNKNOWN, BLOCK_DATA));

	return lzma_block_encoder_init(&coder->next, allocator,
			&coder->block_options);
}


static lzma_ret
stream_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Main loop
	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_STREAM_HEADER_COPY:
	case SEQ_HEADER_METADATA_COPY:
	case SEQ_DATA_COPY:
	case SEQ_FOOTER_METADATA_COPY:
	case SEQ_STREAM_FOOTER_COPY:
		bufcpy(coder->header, &coder->header_pos, coder->header_size,
				out, out_pos, out_size);
		if (coder->header_pos < coder->header_size)
			return LZMA_OK;

		lzma_free(coder->header, allocator);
		coder->header = NULL;

		switch (coder->sequence) {
		case SEQ_STREAM_HEADER_COPY:
			// Write Header Metadata Block if we have Extra for it
			// or known Uncompressed Size.
			if (coder->stream_options->header != NULL
					|| coder->stream_options
						->uncompressed_size
						!= LZMA_VLI_VALUE_UNKNOWN) {
				coder->sequence = SEQ_HEADER_METADATA_INIT;
			} else {
				// Mark that Header Metadata Block doesn't
				// exist.
				if (lzma_info_size_set(coder->info,
						LZMA_INFO_HEADER_METADATA, 0)
						!= LZMA_OK)
					return LZMA_PROG_ERROR;

				coder->sequence = SEQ_DATA_INIT;
			}
			break;

		case SEQ_HEADER_METADATA_COPY:
		case SEQ_DATA_COPY:
		case SEQ_FOOTER_METADATA_COPY:
			++coder->sequence;
			break;

		case SEQ_STREAM_FOOTER_COPY:
			return LZMA_STREAM_END;

		default:
			assert(0);
		}

		break;

	case SEQ_HEADER_METADATA_INIT: {
		lzma_metadata metadata = {
			.header_metadata_size = LZMA_VLI_VALUE_UNKNOWN,
			.total_size = LZMA_VLI_VALUE_UNKNOWN,
			.uncompressed_size = coder->stream_options
					->uncompressed_size,
			.index = NULL,
			.extra = coder->stream_options->header,
		};

		return_if_error(metadata_encoder_init(coder, allocator,
				&metadata, BLOCK_HEADER_METADATA));

		coder->sequence = SEQ_HEADER_METADATA_COPY;
		break;
	}

	case SEQ_FOOTER_METADATA_INIT: {
		lzma_metadata metadata = {
			.header_metadata_size
					= lzma_info_size_get(coder->info,
						LZMA_INFO_HEADER_METADATA),
			.total_size = LZMA_VLI_VALUE_UNKNOWN,
			.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
			.index = lzma_info_index_get(coder->info, false),
			.extra = coder->stream_options->footer,
		};

		return_if_error(metadata_encoder_init(coder, allocator,
				&metadata, BLOCK_FOOTER_METADATA));

		coder->sequence = SEQ_FOOTER_METADATA_COPY;
		break;
	}

	case SEQ_HEADER_METADATA_CODE:
	case SEQ_FOOTER_METADATA_CODE: {
		size_t dummy = 0;
		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, NULL, &dummy, 0,
				out, out_pos, out_size, LZMA_RUN);
		if (ret != LZMA_STREAM_END)
			return ret;

		return_if_error(lzma_info_size_set(coder->info,
				coder->sequence == SEQ_HEADER_METADATA_CODE
					? LZMA_INFO_HEADER_METADATA
					: LZMA_INFO_FOOTER_METADATA,
				coder->block_options.total_size));

		++coder->sequence;
		break;
	}

	case SEQ_DATA_INIT: {
		// Don't create an empty Block unless it would be
		// the only Data Block.
		if (*in_pos == in_size) {
			if (action != LZMA_FINISH)
				return LZMA_OK;

			if (lzma_info_index_count_get(coder->info) != 0) {
				if (lzma_info_index_finish(coder->info))
					return LZMA_DATA_ERROR;

				coder->sequence = SEQ_FOOTER_METADATA_INIT;
				break;
			}
		}

		return_if_error(data_encoder_init(coder, allocator));

		coder->sequence = SEQ_DATA_COPY;
		break;
	}

	case SEQ_DATA_CODE: {
		static const lzma_action convert[4] = {
			LZMA_RUN,
			LZMA_SYNC_FLUSH,
			LZMA_FINISH,
			LZMA_FINISH,
		};

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, convert[action]);
		if (ret != LZMA_STREAM_END || action == LZMA_SYNC_FLUSH)
			return ret;

		return_if_error(lzma_info_iter_set(&coder->iter,
				coder->block_options.total_size,
				coder->block_options.uncompressed_size));

		coder->sequence = SEQ_DATA_INIT;
		break;
	}

	case SEQ_STREAM_FOOTER_INIT: {
		assert(coder->header == NULL);

		lzma_stream_flags flags = {
			.check = coder->stream_options->check,
			.has_crc32 = coder->stream_options->has_crc32,
			.is_multi = true,
		};

		coder->header = lzma_alloc(LZMA_STREAM_TAIL_SIZE, allocator);
		if (coder->header == NULL)
			return LZMA_MEM_ERROR;

		return_if_error(lzma_stream_tail_encode(
				coder->header, &flags));

		coder->header_size = LZMA_STREAM_TAIL_SIZE;
		coder->header_pos = 0;

		coder->sequence = SEQ_STREAM_FOOTER_COPY;
		break;
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
stream_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_info_free(coder->info, allocator);
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

		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->info = NULL;
	} else {
		lzma_free(next->coder->header, allocator);
	}

	next->coder->header = NULL;

	next->coder->info = lzma_info_init(next->coder->info, allocator);
	if (next->coder->info == NULL)
		return LZMA_MEM_ERROR;

	next->coder->sequence = SEQ_STREAM_HEADER_COPY;
	next->coder->stream_options = options;

	// Encode Stream Flags
	{
		lzma_stream_flags flags = {
			.check = options->check,
			.has_crc32 = options->has_crc32,
			.is_multi = true,
		};

		next->coder->header = lzma_alloc(LZMA_STREAM_HEADER_SIZE,
				allocator);
		if (next->coder->header == NULL)
			return LZMA_MEM_ERROR;

		return_if_error(lzma_stream_header_encode(
				next->coder->header, &flags));

		next->coder->header_pos = 0;
		next->coder->header_size = LZMA_STREAM_HEADER_SIZE;
	}

	if (lzma_info_size_set(next->coder->info, LZMA_INFO_STREAM_START,
			options->alignment) != LZMA_OK)
		return LZMA_PROG_ERROR;

	lzma_info_iter_begin(next->coder->info, &next->coder->iter);

	return LZMA_OK;
}


/*
extern lzma_ret
lzma_stream_encoder_multi_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_options_stream *options)
{
	lzma_next_coder_init(stream_encoder_init, next, allocator, options);
}
*/


extern LZMA_API lzma_ret
lzma_stream_encoder_multi(
		lzma_stream *strm, const lzma_options_stream *options)
{
	lzma_next_strm_init(strm, stream_encoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FULL_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
