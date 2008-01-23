///////////////////////////////////////////////////////////////////////////////
//
/// \file       metadata_encoder.c
/// \brief      Encodes metadata to be stored into Metadata Blocks
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

#include "metadata_encoder.h"
#include "block_encoder.h"


struct lzma_coder_s {
	enum {
		SEQ_FLAGS,
		SEQ_HEADER_METADATA_SIZE,
		SEQ_TOTAL_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_INDEX_COUNT,
		SEQ_INDEX_TOTAL,
		SEQ_INDEX_UNCOMPRESSED,
		SEQ_EXTRA_ID,
		SEQ_EXTRA_SIZE,
		SEQ_EXTRA_DATA,
		SEQ_END,
	} sequence;

	/// Position in variable-length integers
	size_t pos;

	/// Local copy of the Metadata structure. Note that we keep
	/// a copy only of the main structure, not Index or Extra Records.
	lzma_metadata metadata;

	/// Number of Records in Index
	size_t index_count;

	/// Index Record currently being processed
	const lzma_index *index_current;

	/// Block encoder for the encoded Metadata
	lzma_next_coder block_encoder;

	/// True once everything except compression has been done.
	bool end_was_reached;

	/// buffer[buffer_pos] is the first byte that needs to be compressed.
	size_t buffer_pos;

	/// buffer[buffer_size] is the next position where a byte will be
	/// written by process().
	size_t buffer_size;

	/// Temporary buffer to which encoded Metadata is written before
	/// it is compressed.
	uint8_t buffer[LZMA_BUFFER_SIZE];
};


#define write_vli(num) \
do { \
	const lzma_ret ret = lzma_vli_encode(num, &coder->pos, 1, \
			coder->buffer, &coder->buffer_size, \
			LZMA_BUFFER_SIZE); \
	if (ret != LZMA_STREAM_END) \
		return ret; \
	coder->pos = 0; \
} while (0)


static lzma_ret
process(lzma_coder *coder)
{
	while (coder->buffer_size < LZMA_BUFFER_SIZE)
	switch (coder->sequence) {
	case SEQ_FLAGS:
		coder->buffer[coder->buffer_size] = 0;

		if (coder->metadata.header_metadata_size != 0)
			coder->buffer[coder->buffer_size] |= 0x01;

		if (coder->metadata.total_size != LZMA_VLI_VALUE_UNKNOWN)
			coder->buffer[coder->buffer_size] |= 0x02;

		if (coder->metadata.uncompressed_size
				!= LZMA_VLI_VALUE_UNKNOWN)
			coder->buffer[coder->buffer_size] |= 0x04;

		if (coder->index_count > 0)
			coder->buffer[coder->buffer_size] |= 0x08;

		if (coder->metadata.extra != NULL)
			coder->buffer[coder->buffer_size] |= 0x80;

		++coder->buffer_size;
		coder->sequence = SEQ_HEADER_METADATA_SIZE;
		break;

	case SEQ_HEADER_METADATA_SIZE:
		if (coder->metadata.header_metadata_size != 0)
			write_vli(coder->metadata.header_metadata_size);

		coder->sequence = SEQ_TOTAL_SIZE;
		break;

	case SEQ_TOTAL_SIZE:
		if (coder->metadata.total_size != LZMA_VLI_VALUE_UNKNOWN)
			write_vli(coder->metadata.total_size);

		coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		break;

	case SEQ_UNCOMPRESSED_SIZE:
		if (coder->metadata.uncompressed_size
				!= LZMA_VLI_VALUE_UNKNOWN)
			write_vli(coder->metadata.uncompressed_size);

		coder->sequence = SEQ_INDEX_COUNT;
		break;

	case SEQ_INDEX_COUNT:
		if (coder->index_count == 0) {
			if (coder->metadata.extra == NULL) {
				coder->sequence = SEQ_END;
				return LZMA_STREAM_END;
			}

			coder->sequence = SEQ_EXTRA_ID;
			break;
		}

		write_vli(coder->index_count);
		coder->sequence = SEQ_INDEX_TOTAL;
		break;

	case SEQ_INDEX_TOTAL:
		write_vli(coder->index_current->total_size);

		coder->index_current = coder->index_current->next;
		if (coder->index_current == NULL) {
			coder->index_current = coder->metadata.index;
			coder->sequence = SEQ_INDEX_UNCOMPRESSED;
		}

		break;

	case SEQ_INDEX_UNCOMPRESSED:
		write_vli(coder->index_current->uncompressed_size);

		coder->index_current = coder->index_current->next;
		if (coder->index_current != NULL)
			break;

		if (coder->metadata.extra != NULL) {
			coder->sequence = SEQ_EXTRA_ID;
			break;
		}

		coder->sequence = SEQ_END;
		return LZMA_STREAM_END;

	case SEQ_EXTRA_ID: {
		const lzma_ret ret = lzma_vli_encode(
				coder->metadata.extra->id, &coder->pos, 1,
				coder->buffer, &coder->buffer_size,
				LZMA_BUFFER_SIZE);
		switch (ret) {
		case LZMA_OK:
			break;

		case LZMA_STREAM_END:
			coder->pos = 0;

			// Handle the special ID 0.
			if (coder->metadata.extra->id == 0) {
				coder->metadata.extra
						= coder->metadata.extra->next;
				if (coder->metadata.extra == NULL) {
					coder->sequence = SEQ_END;
					return LZMA_STREAM_END;
				}

				coder->sequence = SEQ_EXTRA_ID;

			} else {
				coder->sequence = SEQ_EXTRA_SIZE;
			}

			break;

		default:
			return ret;
		}

		break;
	}

	case SEQ_EXTRA_SIZE:
		if (coder->metadata.extra->size >= (lzma_vli)(SIZE_MAX))
			return LZMA_HEADER_ERROR;

		write_vli(coder->metadata.extra->size);
		coder->sequence = SEQ_EXTRA_DATA;
		break;

	case SEQ_EXTRA_DATA:
		bufcpy(coder->metadata.extra->data, &coder->pos,
				coder->metadata.extra->size,
				coder->buffer, &coder->buffer_size,
				LZMA_BUFFER_SIZE);

		if ((size_t)(coder->metadata.extra->size) == coder->pos) {
			coder->metadata.extra = coder->metadata.extra->next;
			if (coder->metadata.extra == NULL) {
				coder->sequence = SEQ_END;
				return LZMA_STREAM_END;
			}

			coder->pos = 0;
			coder->sequence = SEQ_EXTRA_ID;
		}

		break;

	case SEQ_END:
		// Everything is encoded. Let the compression code finish
		// its work now.
		return LZMA_STREAM_END;
	}

	return LZMA_OK;
}


static lzma_ret
metadata_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in lzma_attribute((unused)),
		size_t *restrict in_pos lzma_attribute((unused)),
		size_t in_size lzma_attribute((unused)), uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action lzma_attribute((unused)))
{
	while (!coder->end_was_reached) {
		// Flush coder->buffer if it isn't empty.
		if (coder->buffer_size > 0) {
			const lzma_ret ret = coder->block_encoder.code(
					coder->block_encoder.coder, allocator,
					coder->buffer, &coder->buffer_pos,
					coder->buffer_size,
					out, out_pos, out_size, LZMA_RUN);
			if (coder->buffer_pos < coder->buffer_size
					|| ret != LZMA_OK)
				return ret;

			coder->buffer_pos = 0;
			coder->buffer_size = 0;
		}

		const lzma_ret ret = process(coder);

		switch (ret) {
		case LZMA_OK:
			break;

		case LZMA_STREAM_END:
			coder->end_was_reached = true;
			break;

		default:
			return ret;
		}
	}

	// Finish
	return coder->block_encoder.code(coder->block_encoder.coder, allocator,
			coder->buffer, &coder->buffer_pos, coder->buffer_size,
			out, out_pos, out_size, LZMA_FINISH);
}


static void
metadata_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->block_encoder, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
metadata_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, const lzma_metadata *metadata)
{
	if (options == NULL || metadata == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &metadata_encode;
		next->end = &metadata_encoder_end;
		next->coder->block_encoder = LZMA_NEXT_CODER_INIT;
	}

	next->coder->sequence = SEQ_FLAGS;
	next->coder->pos = 0;
	next->coder->metadata = *metadata;
	next->coder->index_count = 0;
	next->coder->index_current = metadata->index;
	next->coder->end_was_reached = false;
	next->coder->buffer_pos = 0;
	next->coder->buffer_size = 0;

	// Count and validate the Index Records.
	{
		const lzma_index *i = metadata->index;
		while (i != NULL) {
			if (i->total_size > LZMA_VLI_VALUE_MAX
					|| i->uncompressed_size
						> LZMA_VLI_VALUE_MAX)
				return LZMA_PROG_ERROR;

			++next->coder->index_count;
			i = i->next;
		}
	}

	// Initialize the Block encoder.
	return lzma_block_encoder_init(
			&next->coder->block_encoder, allocator, options);
}


extern lzma_ret
lzma_metadata_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, const lzma_metadata *metadata)
{
	lzma_next_coder_init(metadata_encoder_init, next, allocator,
			options, metadata);
}


extern LZMA_API lzma_ret
lzma_metadata_encoder(lzma_stream *strm, lzma_options_block *options,
		const lzma_metadata *metadata)
{
	lzma_next_strm_init(strm, metadata_encoder_init, options, metadata);

	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}


extern LZMA_API lzma_vli
lzma_metadata_size(const lzma_metadata *metadata)
{
	lzma_vli size = 1; // Metadata Flags

	// Validate header_metadata_size, total_size, and uncompressed_size.
	if (metadata->header_metadata_size > LZMA_VLI_VALUE_MAX
			|| !lzma_vli_is_valid(metadata->total_size)
			|| metadata->total_size == 0
			|| !lzma_vli_is_valid(metadata->uncompressed_size))
		return 0;

	// Add the sizes of these three fields.
	if (metadata->header_metadata_size != 0)
		size += lzma_vli_size(metadata->header_metadata_size);

	if (metadata->total_size != LZMA_VLI_VALUE_UNKNOWN)
		size += lzma_vli_size(metadata->total_size);

	if (metadata->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		size += lzma_vli_size(metadata->uncompressed_size);

	// Index
	if (metadata->index != NULL) {
		const lzma_index *i = metadata->index;
		size_t count = 1;

		do {
			const size_t x = lzma_vli_size(i->total_size);
			const size_t y = lzma_vli_size(i->uncompressed_size);
			if (x == 0 || y == 0)
				return 0;

			size += x + y;
			++count;
			i = i->next;

		} while (i != NULL);

		const size_t tmp = lzma_vli_size(count);
		if (tmp == 0)
			return 0;

		size += tmp;
	}

	// Extra
	{
		const lzma_extra *e = metadata->extra;
		while (e != NULL) {
			// Validate the numbers.
			if (e->id > LZMA_VLI_VALUE_MAX
					|| e->size >= (lzma_vli)(SIZE_MAX))
				return 0;

			// Add the sizes.
			size += lzma_vli_size(e->id);
			if (e->id != 0) {
				size += lzma_vli_size(e->size);
				size += e->size;
			}

			e = e->next;
		}
	}

	return size;
}
