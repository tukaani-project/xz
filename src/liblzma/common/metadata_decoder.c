///////////////////////////////////////////////////////////////////////////////
//
/// \file       metadata_decoder.c
/// \brief      Decodes metadata stored in Metadata Blocks
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

#include "metadata_decoder.h"
#include "block_decoder.h"


/// Maximum size of a single Extra Record. Again, this is mostly to make
/// sure that the parsed lzma_vli fits into size_t. Still, maybe this should
/// be smaller.
#define EXTRA_SIZE_MAX (SIZE_MAX / 4)


struct lzma_coder_s {
	enum {
		SEQ_FLAGS,
		SEQ_HEADER_METADATA_SIZE,
		SEQ_TOTAL_SIZE,
		SEQ_UNCOMPRESSED_SIZE,
		SEQ_INDEX_COUNT,
		SEQ_INDEX_ALLOC,
		SEQ_INDEX_TOTAL_SIZE,
		SEQ_INDEX_UNCOMPRESSED_SIZE,
		SEQ_EXTRA_PREPARE,
		SEQ_EXTRA_ALLOC,
		SEQ_EXTRA_ID,
		SEQ_EXTRA_SIZE,
		SEQ_EXTRA_DATA_ALLOC,
		SEQ_EXTRA_DATA_COPY,
		SEQ_EXTRA_DUMMY_ALLOC,
		SEQ_EXTRA_DUMMY_ID,
		SEQ_EXTRA_DUMMY_SIZE,
		SEQ_EXTRA_DUMMY_COPY,
	} sequence;

	/// Number of "things" left to be parsed. If we hit end of input
	/// when this isn't zero, we have corrupt Metadata Block.
	size_t todo_count;

	/// Position in variable-length integers
	size_t pos;

	/// Temporary variable needed to decode variables whose type
	/// is size_t instead of lzma_vli.
	lzma_vli tmp;

	/// Pointer to target structure to hold the parsed results.
	lzma_metadata *metadata;

	/// The Index Record we currently are parsing
	lzma_index *index_current;

	/// Number of Records in Index
	size_t index_count;

	/// Sum of Total Size fields in the Index
	lzma_vli index_total_size;

	/// Sum of Uncompressed Size fields in the Index
	lzma_vli index_uncompressed_size;

	/// True if Extra is present.
	bool has_extra;

	/// True if we have been requested to store the Extra to *metadata.
	bool want_extra;

	/// Pointer to the end of the Extra Record list.
	lzma_extra *extra_tail;

	/// Dummy Extra Record used when only verifying integrity of Extra
	/// (not storing it to RAM).
	lzma_extra extra_dummy;

	/// Block decoder
	lzma_next_coder block_decoder;

	/// buffer[buffer_pos] is the next byte to process.
	size_t buffer_pos;

	/// buffer[buffer_size] is the first byte to not process.
	size_t buffer_size;

	/// Temporary buffer to which encoded Metadata is read before
	/// it is parsed.
	uint8_t buffer[LZMA_BUFFER_SIZE];
};


/// Reads a variable-length integer to coder->num.
#define read_vli(num) \
do { \
	const lzma_ret ret = lzma_vli_decode( \
			&num, &coder->pos, \
			coder->buffer, &coder->buffer_pos, \
			coder->buffer_size); \
	if (ret != LZMA_STREAM_END) \
		return ret; \
	\
	coder->pos = 0; \
} while (0)


static lzma_ret
process(lzma_coder *coder, lzma_allocator *allocator)
{
	while (coder->buffer_pos < coder->buffer_size)
	switch (coder->sequence) {
	case SEQ_FLAGS:
		// Reserved bits must be unset.
		if (coder->buffer[coder->buffer_pos] & 0x70)
			return LZMA_HEADER_ERROR;

		// If Size of Header Metadata is present, prepare the
		// variable for variable-length integer decoding. Otherwise
		// set it to LZMA_VLI_VALUE_UNKNOWN to indicate that the
		// field isn't present.
		if (coder->buffer[coder->buffer_pos] & 0x01) {
			coder->metadata->header_metadata_size = 0;
			++coder->todo_count;
		}

		if (coder->buffer[coder->buffer_pos] & 0x02) {
			coder->metadata->total_size = 0;
			++coder->todo_count;
		}

		if (coder->buffer[coder->buffer_pos] & 0x04) {
			coder->metadata->uncompressed_size = 0;
			++coder->todo_count;
		}

		if (coder->buffer[coder->buffer_pos] & 0x08) {
			// Setting index_count to 1 is just to indicate that
			// Index is present. The real size is parsed later.
			coder->index_count = 1;
			++coder->todo_count;
		}

		coder->has_extra = (coder->buffer[coder->buffer_pos] & 0x80)
				!= 0;

		++coder->buffer_pos;
		coder->sequence = SEQ_HEADER_METADATA_SIZE;
		break;

	case SEQ_HEADER_METADATA_SIZE:
		if (coder->metadata->header_metadata_size
				!= LZMA_VLI_VALUE_UNKNOWN) {
			read_vli(coder->metadata->header_metadata_size);

			if (coder->metadata->header_metadata_size == 0)
				return LZMA_DATA_ERROR;

			--coder->todo_count;
		} else {
			// Zero indicates that Size of Header Metadata Block
			// is not present. That is, after successful Metadata
			// decoding, metadata->header_metadata_size is
			// never LZMA_VLI_VALUE_UNKNOWN.
			coder->metadata->header_metadata_size = 0;
		}

		coder->sequence = SEQ_TOTAL_SIZE;
		break;

	case SEQ_TOTAL_SIZE:
		if (coder->metadata->total_size != LZMA_VLI_VALUE_UNKNOWN) {
			read_vli(coder->metadata->total_size);

			if (coder->metadata->total_size == 0)
				return LZMA_DATA_ERROR;

			--coder->todo_count;
		}

		coder->sequence = SEQ_UNCOMPRESSED_SIZE;
		break;

	case SEQ_UNCOMPRESSED_SIZE:
		if (coder->metadata->uncompressed_size
				!= LZMA_VLI_VALUE_UNKNOWN) {
			read_vli(coder->metadata->uncompressed_size);
			--coder->todo_count;
		}

		coder->sequence = SEQ_INDEX_COUNT;
		break;

	case SEQ_INDEX_COUNT:
		if (coder->index_count == 0) {
			coder->sequence = SEQ_EXTRA_PREPARE;
			break;
		}

		read_vli(coder->tmp);

		// Index must not be empty nor far too big (wouldn't fit
		// in RAM).
		if (coder->tmp == 0 || coder->tmp
				>= SIZE_MAX / sizeof(lzma_index))
			return LZMA_DATA_ERROR;

		coder->index_count = (size_t)(coder->tmp);
		coder->tmp = 0;

		coder->sequence = SEQ_INDEX_ALLOC;
		break;

	case SEQ_INDEX_ALLOC: {
		lzma_index *i = lzma_alloc(sizeof(lzma_index), allocator);
		if (i == NULL)
			return LZMA_MEM_ERROR;

		i->total_size = 0;
		i->uncompressed_size = 0;
		i->next = NULL;

		if (coder->metadata->index == NULL)
			coder->metadata->index = i;
		else
			coder->index_current->next = i;

		coder->index_current = i;

		coder->sequence = SEQ_INDEX_TOTAL_SIZE;
	}

	// Fall through

	case SEQ_INDEX_TOTAL_SIZE: {
		read_vli(coder->index_current->total_size);

		coder->index_total_size += coder->index_current->total_size;
		if (coder->index_total_size > LZMA_VLI_VALUE_MAX)
			return LZMA_DATA_ERROR;

		// No Block can have Total Size of zero bytes.
		if (coder->index_current->total_size == 0)
			return LZMA_DATA_ERROR;

		if (--coder->index_count == 0) {
			// If Total Size is present, it must match the sum
			// of Total Sizes in Index.
			if (coder->metadata->total_size
						!= LZMA_VLI_VALUE_UNKNOWN
					&& coder->metadata->total_size
						!= coder->index_total_size)
				return LZMA_DATA_ERROR;

			coder->index_current = coder->metadata->index;
			coder->sequence = SEQ_INDEX_UNCOMPRESSED_SIZE;
		} else {
			coder->sequence = SEQ_INDEX_ALLOC;
		}

		break;
	}

	case SEQ_INDEX_UNCOMPRESSED_SIZE: {
		read_vli(coder->index_current->uncompressed_size);

		coder->index_uncompressed_size
				+= coder->index_current->uncompressed_size;
		if (coder->index_uncompressed_size > LZMA_VLI_VALUE_MAX)
			return LZMA_DATA_ERROR;

		coder->index_current = coder->index_current->next;
		if (coder->index_current == NULL) {
			if (coder->metadata->uncompressed_size
						!= LZMA_VLI_VALUE_UNKNOWN
					&& coder->metadata->uncompressed_size
					!= coder->index_uncompressed_size)
				return LZMA_DATA_ERROR;

			--coder->todo_count;
			coder->sequence = SEQ_EXTRA_PREPARE;
		}

		break;
	}

	case SEQ_EXTRA_PREPARE:
		assert(coder->todo_count == 0);

		// If we get here, we have at least one byte of input left.
		// If "Extra is present" flag is unset in Metadata Flags,
		// it means that there is some garbage and we return an error.
		if (!coder->has_extra)
			return LZMA_DATA_ERROR;

		if (!coder->want_extra) {
			coder->extra_tail = &coder->extra_dummy;
			coder->sequence = SEQ_EXTRA_DUMMY_ALLOC;
			break;
		}

		coder->sequence = SEQ_EXTRA_ALLOC;

	// Fall through

	case SEQ_EXTRA_ALLOC: {
		lzma_extra *e = lzma_alloc(sizeof(lzma_extra), allocator);
		if (e == NULL)
			return LZMA_MEM_ERROR;

		e->next = NULL;
		e->id = 0;
		e->size = 0;
		e->data = NULL;

		if (coder->metadata->extra == NULL)
			coder->metadata->extra = e;
		else
			coder->extra_tail->next = e;

		coder->extra_tail = e;

		coder->todo_count = 1;
		coder->sequence = SEQ_EXTRA_ID;
	}

	// Fall through

	case SEQ_EXTRA_ID:
	case SEQ_EXTRA_DUMMY_ID:
		read_vli(coder->extra_tail->id);

		if (coder->extra_tail->id == 0) {
			coder->extra_tail->size = 0;
			coder->extra_tail->data = NULL;
			coder->todo_count = 0;
			--coder->sequence;
		} else {
			++coder->sequence;
		}

		break;

	case SEQ_EXTRA_SIZE:
	case SEQ_EXTRA_DUMMY_SIZE:
		read_vli(coder->tmp);
		++coder->sequence;
		break;

	case SEQ_EXTRA_DATA_ALLOC: {
		if (coder->tmp > EXTRA_SIZE_MAX)
			return LZMA_DATA_ERROR;

		coder->extra_tail->size = (size_t)(coder->tmp);
		coder->tmp = 0;

		uint8_t *d = lzma_alloc((size_t)(coder->extra_tail->size),
				allocator);
		if (d == NULL)
			return LZMA_MEM_ERROR;

		coder->extra_tail->data = d;
		coder->sequence = SEQ_EXTRA_DATA_COPY;
	}

	// Fall through

	case SEQ_EXTRA_DATA_COPY:
		bufcpy(coder->buffer, &coder->buffer_pos, coder->buffer_size,
				coder->extra_tail->data, &coder->pos,
				(size_t)(coder->extra_tail->size));

		if ((size_t)(coder->extra_tail->size) == coder->pos) {
			coder->pos = 0;
			coder->todo_count = 0;
			coder->sequence = SEQ_EXTRA_ALLOC;
		}

		break;

	case SEQ_EXTRA_DUMMY_ALLOC:
		// Not really alloc, just initialize the dummy entry.
		coder->extra_dummy = (lzma_extra){
			.next = NULL,
			.id = 0,
			.size = 0,
			.data = NULL,
		};

		coder->todo_count = 1;
		coder->sequence = SEQ_EXTRA_DUMMY_ID;
		break;

	case SEQ_EXTRA_DUMMY_COPY: {
		// Simply skip as many bytes as indicated by Extra Record Size.
		// We don't check lzma_extra_size_max because we don't
		// allocate any memory to hold the data.
		const size_t in_avail = coder->buffer_size - coder->buffer_pos;
		const size_t skip = MIN((lzma_vli)(in_avail), coder->tmp);
		coder->buffer_pos += skip;
		coder->tmp -= skip;

		if (coder->tmp == 0) {
			coder->todo_count = 0;
			coder->sequence = SEQ_EXTRA_DUMMY_ALLOC;
		}

		break;
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
metadata_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	bool end_was_reached = false;

	while (true) {
		// Fill the buffer if it is empty.
		if (coder->buffer_pos == coder->buffer_size) {
			coder->buffer_pos = 0;
			coder->buffer_size = 0;

			const lzma_ret ret = coder->block_decoder.code(
					coder->block_decoder.coder, allocator,
					in, in_pos, in_size, coder->buffer,
					&coder->buffer_size, LZMA_BUFFER_SIZE,
					LZMA_RUN);

			switch (ret) {
			case LZMA_OK:
				// Return immediatelly if we got no new data.
				if (coder->buffer_size == 0)
					return LZMA_OK;

				break;

			case LZMA_STREAM_END:
				end_was_reached = true;
				break;

			default:
				return ret;
			}
		}

		// Process coder->buffer.
		const lzma_ret ret = process(coder, allocator);
		if (ret != LZMA_OK)
			return ret;

		// On success, process() eats all the input.
		assert(coder->buffer_pos == coder->buffer_size);

		if (end_was_reached) {
			// Check that the sequence is not in the
			// middle of anything.
			if (coder->todo_count != 0)
				return LZMA_DATA_ERROR;

			return LZMA_STREAM_END;
		}
	}
}


static void
metadata_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->block_decoder, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
metadata_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, lzma_metadata *metadata,
		bool want_extra)
{
	if (options == NULL || metadata == NULL)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &metadata_decode;
		next->end = &metadata_decoder_end;
		next->coder->block_decoder = LZMA_NEXT_CODER_INIT;
	}

	metadata->header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	metadata->total_size = LZMA_VLI_VALUE_UNKNOWN;
	metadata->uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	metadata->index = NULL;
	metadata->extra = NULL;

	next->coder->sequence = SEQ_FLAGS;
	next->coder->todo_count = 0;
	next->coder->pos = 0;
	next->coder->tmp = 0;
	next->coder->metadata = metadata;
	next->coder->index_current = NULL;
	next->coder->index_count = 0;
	next->coder->index_total_size = 0;
	next->coder->index_uncompressed_size = 0;
	next->coder->want_extra = want_extra;
	next->coder->extra_tail = NULL;
	next->coder->buffer_pos = 0;
	next->coder->buffer_size = 0;

	return lzma_block_decoder_init(
			&next->coder->block_decoder, allocator, options);
}


extern lzma_ret
lzma_metadata_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, lzma_metadata *metadata,
		bool want_extra)
{
	lzma_next_coder_init(metadata_decoder_init, next, allocator,
			options, metadata, want_extra);
}


extern LZMA_API lzma_ret
lzma_metadata_decoder(lzma_stream *strm, lzma_options_block *options,
		lzma_metadata *metadata, lzma_bool want_extra)
{
	lzma_next_strm_init(strm, lzma_metadata_decoder_init,
			options, metadata, want_extra);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
