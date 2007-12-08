///////////////////////////////////////////////////////////////////////////////
//
/// \file       info.c
/// \brief      Collects and verifies integrity of Stream size information
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


struct lzma_info_s {
	struct {
		/// Known Size of Header Metadata Block; here's some
		/// special things:
		///  - LZMA_VLI_VALUE_UNKNOWN indicates that we don't know
		///    if Header Metadata Block is present.
		///  - 0 indicates that Header Metadata Block is not present.
		lzma_vli header_metadata_size;

		/// Known Total Size of the Data Blocks in the Stream
		lzma_vli total_size;

		/// Known Uncompressed Size of the Data Blocks in the Stream
		lzma_vli uncompressed_size;

		/// Known Size of Footer Metadata Block
		lzma_vli footer_metadata_size;
	} known;

	struct {
		/// Sum of Total Size fields stored to the Index so far
		lzma_vli total_size;

		/// Sum of Uncompressed Size fields stored to the Index so far
		lzma_vli uncompressed_size;

		/// First Index Record in the list, or NULL if Index is empty.
		lzma_index *head;

		/// Number of Index Records
		size_t record_count;

		/// Number of Index Records
		size_t incomplete_count;

		/// True when we know that no more Records will get added
		/// to the Index.
		bool is_final;
	} index;

	/// Start offset of the Stream. This is needed to calculate
	/// lzma_info_iter.stream_offset.
	lzma_vli stream_start_offset;

	/// True if Index is present in Header Metadata Block
	bool has_index_in_header_metadata;
};


//////////////////////
// Create/Reset/End //
//////////////////////

static void
index_init(lzma_info *info)
{
	info->index.total_size = 0;
	info->index.uncompressed_size = 0;
	info->index.head = NULL;
	info->index.record_count = 0;
	info->index.incomplete_count = 0;
	info->index.is_final = false;
	return;
}


static void
info_init(lzma_info *info)
{
	info->known.header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	info->known.total_size = LZMA_VLI_VALUE_UNKNOWN;
	info->known.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	info->known.footer_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	info->stream_start_offset = 0;
	info->has_index_in_header_metadata = false;

	index_init(info);

	return;
}


extern LZMA_API lzma_info *
lzma_info_init(lzma_info *info, lzma_allocator *allocator)
{
	if (info == NULL)
		info = lzma_alloc(sizeof(lzma_info), allocator);
	else
		lzma_index_free(info->index.head, allocator);

	if (info != NULL)
		info_init(info);

	return info;
}


extern LZMA_API void
lzma_info_free(lzma_info *info, lzma_allocator *allocator)
{
	lzma_index_free(info->index.head, allocator);
	lzma_free(info, allocator);
	return;
}


/////////
// Set //
/////////

static lzma_ret
set_size(lzma_vli new_size, lzma_vli *known_size, lzma_vli index_size,
		bool forbid_zero)
{
	assert(new_size <= LZMA_VLI_VALUE_MAX);

	lzma_ret ret = LZMA_OK;

	if (forbid_zero && new_size == 0)
		ret = LZMA_PROG_ERROR;
	else if (index_size > new_size)
		ret = LZMA_DATA_ERROR;
	else if (*known_size == LZMA_VLI_VALUE_UNKNOWN)
		*known_size = new_size;
	else if (*known_size != new_size)
		ret = LZMA_DATA_ERROR;

	return ret;
}


extern LZMA_API lzma_ret
lzma_info_size_set(lzma_info *info, lzma_info_size type, lzma_vli size)
{
	if (size > LZMA_VLI_VALUE_MAX)
		return LZMA_PROG_ERROR;

	switch (type) {
	case LZMA_INFO_STREAM_START:
		info->stream_start_offset = size;
		return LZMA_OK;

	case LZMA_INFO_HEADER_METADATA:
		return set_size(size, &info->known.header_metadata_size,
				0, false);

	case LZMA_INFO_TOTAL:
		return set_size(size, &info->known.total_size,
				info->index.total_size, true);

	case LZMA_INFO_UNCOMPRESSED:
		return set_size(size, &info->known.uncompressed_size,
				info->index.uncompressed_size, false);

	case LZMA_INFO_FOOTER_METADATA:
		return set_size(size, &info->known.footer_metadata_size,
				0, true);
	}

	return LZMA_PROG_ERROR;
}


extern LZMA_API lzma_ret
lzma_info_index_set(lzma_info *info, lzma_allocator *allocator,
		lzma_index *i_new, lzma_bool eat_index)
{
	if (i_new == NULL)
		return LZMA_PROG_ERROR;

	lzma_index *i_old = info->index.head;

	if (i_old != NULL) {
		while (true) {
			// If the new Index has fewer Records than the old one,
			// the new Index cannot be valid.
			if (i_new == NULL)
				return LZMA_DATA_ERROR;

			// The new Index must be complete i.e. no unknown
			// values.
			if (i_new->total_size > LZMA_VLI_VALUE_MAX
					|| i_new->uncompressed_size
						> LZMA_VLI_VALUE_MAX) {
				if (eat_index)
					lzma_index_free(i_new, allocator);

				return LZMA_PROG_ERROR;
			}

			// Compare the values from the new Index with the old
			// Index. The old Index may be incomplete; in that
			// case we
			//  - use the value from the new Index as is;
			//  - update the appropriate info->index.foo_size; and
			//  - decrease the count of incomplete Index Records.
			bool was_incomplete = false;

			if (i_old->total_size == LZMA_VLI_VALUE_UNKNOWN) {
				assert(!info->index.is_final);
				was_incomplete = true;

				i_old->total_size = i_new->total_size;

				if (lzma_vli_add(info->index.total_size,
						i_new->total_size)) {
					if (eat_index)
						lzma_index_free(i_new,
								allocator);

					return LZMA_PROG_ERROR;
				}
			} else if (i_old->total_size != i_new->total_size) {
				if (eat_index)
					lzma_index_free(i_new, allocator);

				return LZMA_DATA_ERROR;
			}

			if (i_old->uncompressed_size
					== LZMA_VLI_VALUE_UNKNOWN) {
				assert(!info->index.is_final);
				was_incomplete = true;

				i_old->uncompressed_size
						= i_new->uncompressed_size;

				if (lzma_vli_add(info->index.uncompressed_size,
						i_new->uncompressed_size)) {
					if (eat_index)
						lzma_index_free(i_new,
								allocator);

					return LZMA_PROG_ERROR;
				}
			} else if (i_old->uncompressed_size
					!= i_new->uncompressed_size) {
				if (eat_index)
					lzma_index_free(i_new, allocator);

				return LZMA_DATA_ERROR;
			}

			if (was_incomplete) {
				assert(!info->index.is_final);
				assert(info->index.incomplete_count > 0);
				--info->index.incomplete_count;
			}

			// Get rid of *i_new. It's now identical with *i_old.
			lzma_index *tmp = i_new->next;
			if (eat_index)
				lzma_free(i_new, allocator);

			i_new = tmp;

			// We want to leave i_old pointing to the last
			// Index Record in the old Index. This way we can
			// concatenate the possible new Records from i_new.
			if (i_old->next == NULL)
				break;

			i_old = i_old->next;
		}
	}

	assert(info->index.incomplete_count == 0);

	// If Index was already known to be final, i_new must be NULL now.
	// The new Index cannot contain more Records that we already have.
	if (info->index.is_final) {
		assert(info->index.head != NULL);

		if (i_new != NULL) {
			if (eat_index)
				lzma_index_free(i_new, allocator);

			return LZMA_DATA_ERROR;
		}

		return LZMA_OK;
	}

	// The rest of the new Index is merged to the old Index. Keep the
	// current i_new pointer in available. We need it when merging the
	// new Index with the old one, and if an error occurs so we can
	// get rid of the broken part of the new Index.
	lzma_index *i_start = i_new;
	while (i_new != NULL) {
		// The new Index must be complete i.e. no unknown values.
		if (i_new->total_size > LZMA_VLI_VALUE_MAX
				|| i_new->uncompressed_size
					> LZMA_VLI_VALUE_MAX) {
			if (eat_index)
				lzma_index_free(i_start, allocator);

			return LZMA_PROG_ERROR;
		}

		// Update info->index.foo_sizes.
		if (lzma_vli_add(info->index.total_size, i_new->total_size)
				|| lzma_vli_add(info->index.uncompressed_size,
					i_new->uncompressed_size)) {
			if (eat_index)
				lzma_index_free(i_start, allocator);

			return LZMA_PROG_ERROR;
		}

		++info->index.record_count;
		i_new = i_new->next;
	}

	// All the Records in the new Index are good, and info->index.foo_sizes
	// were successfully updated.
	if (lzma_info_index_finish(info) != LZMA_OK) {
		if (eat_index)
			lzma_index_free(i_start, allocator);

		return LZMA_DATA_ERROR;
	}

	// The Index is ready to be merged. If we aren't supposed to eat
	// the Index, make a copy of it first.
	if (!eat_index && i_start != NULL) {
		i_start = lzma_index_dup(i_start, allocator);
		if (i_start == NULL)
			return LZMA_MEM_ERROR;
	}

	// Concatenate the new Index with the old one. Note that it is
	// possible that we don't have any old Index.
	if (info->index.head == NULL)
		info->index.head = i_start;
	else
		i_old->next = i_start;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_info_metadata_set(lzma_info *info, lzma_allocator *allocator,
		lzma_metadata *metadata, lzma_bool is_header_metadata,
		lzma_bool eat_index)
{
	// Validate *metadata.
	if (!lzma_vli_is_valid(metadata->header_metadata_size)
			|| !lzma_vli_is_valid(metadata->total_size)
			|| !lzma_vli_is_valid(metadata->uncompressed_size)) {
		if (eat_index) {
			lzma_index_free(metadata->index, allocator);
			metadata->index = NULL;
		}

		return LZMA_PROG_ERROR;
	}

	// Index
	if (metadata->index != NULL) {
		if (is_header_metadata)
			info->has_index_in_header_metadata = true;

		const lzma_ret ret = lzma_info_index_set(
				info, allocator, metadata->index, eat_index);
		if (ret != LZMA_OK)
			return ret;

	} else if (!is_header_metadata
			&& (metadata->total_size == LZMA_VLI_VALUE_UNKNOWN
				|| !info->has_index_in_header_metadata)) {
		// Either Total Size or Index must be present in Footer
		// Metadata Block. If Index is not present, it must have
		// already been in the Header Metadata Block. Since we
		// got here, these conditions weren't met.
		return LZMA_DATA_ERROR;
	}

	// Size of Header Metadata
	if (!is_header_metadata) {
		// If it is marked unknown in Metadata, it means that
		// it's not present.
		const lzma_vli size = metadata->header_metadata_size
					!= LZMA_VLI_VALUE_UNKNOWN
				? metadata->header_metadata_size : 0;
		const lzma_ret ret = lzma_info_size_set(
				info, LZMA_INFO_HEADER_METADATA, size);
		if (ret != LZMA_OK)
			return ret;
	}

	// Total Size
	if (metadata->total_size != LZMA_VLI_VALUE_UNKNOWN) {
		const lzma_ret ret = lzma_info_size_set(info,
				LZMA_INFO_TOTAL, metadata->total_size);
		if (ret != LZMA_OK)
			return ret;
	}

	// Uncompressed Size
	if (metadata->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		const lzma_ret ret = lzma_info_size_set(info,
				LZMA_INFO_UNCOMPRESSED,
				metadata->uncompressed_size);
		if (ret != LZMA_OK)
			return ret;
	}

	return LZMA_OK;
}


/////////
// Get //
/////////

extern LZMA_API lzma_vli
lzma_info_size_get(const lzma_info *info, lzma_info_size type)
{
	switch (type) {
	case LZMA_INFO_STREAM_START:
		return info->stream_start_offset;

	case LZMA_INFO_HEADER_METADATA:
		return info->known.header_metadata_size;

	case LZMA_INFO_TOTAL:
		return info->known.total_size;

	case LZMA_INFO_UNCOMPRESSED:
		return info->known.uncompressed_size;

	case LZMA_INFO_FOOTER_METADATA:
		return info->known.footer_metadata_size;
	}

	return LZMA_VLI_VALUE_UNKNOWN;
}


extern LZMA_API lzma_index *
lzma_info_index_get(lzma_info *info, lzma_bool detach)
{
	lzma_index *i = info->index.head;

	if (detach)
		index_init(info);

	return i;
}


extern LZMA_API size_t
lzma_info_index_count_get(const lzma_info *info)
{
	return info->index.record_count;
}


/////////////////
// Incremental //
/////////////////

enum {
	ITER_INFO,
	ITER_INDEX,
	ITER_RESERVED_1,
	ITER_RESERVED_2,
};


#define iter_info ((lzma_info *)(iter->internal[ITER_INFO]))

#define iter_index ((lzma_index *)(iter->internal[ITER_INDEX]))


extern LZMA_API void
lzma_info_iter_begin(lzma_info *info, lzma_info_iter *iter)
{
	*iter = (lzma_info_iter){
		.total_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.stream_offset = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_offset = LZMA_VLI_VALUE_UNKNOWN,
		.internal = { info, NULL, NULL, NULL },
	};

	return;
}


extern LZMA_API lzma_ret
lzma_info_iter_next(lzma_info_iter *iter, lzma_allocator *allocator)
{
	// FIXME debug remove
	lzma_info *info = iter_info;
	(void)info;

	if (iter_index == NULL) {
		// The first call after lzma_info_iter_begin().
		if (iter_info->known.header_metadata_size
				== LZMA_VLI_VALUE_UNKNOWN)
			iter->stream_offset = LZMA_VLI_VALUE_UNKNOWN;
		else if (lzma_vli_sum3(iter->stream_offset,
				iter_info->stream_start_offset,
				LZMA_STREAM_HEADER_SIZE,
				iter_info->known.header_metadata_size))
			return LZMA_PROG_ERROR;

		iter->uncompressed_offset = 0;

		if (iter_info->index.head != NULL) {
			// The first Index Record has already been allocated.
			iter->internal[ITER_INDEX] = iter_info->index.head;
			iter->total_size = iter_index->total_size;
			iter->uncompressed_size
					= iter_index->uncompressed_size;
			return LZMA_OK;
		}
	} else {
		// Update iter->*_offsets.
		if (iter->stream_offset != LZMA_VLI_VALUE_UNKNOWN) {
			if (iter_index->total_size == LZMA_VLI_VALUE_UNKNOWN)
				iter->stream_offset = LZMA_VLI_VALUE_UNKNOWN;
			else if (lzma_vli_add(iter->stream_offset,
					iter_index->total_size))
				return LZMA_DATA_ERROR;
		}

		if (iter->uncompressed_offset != LZMA_VLI_VALUE_UNKNOWN) {
			if (iter_index->uncompressed_size
					== LZMA_VLI_VALUE_UNKNOWN)
				iter->uncompressed_offset
						= LZMA_VLI_VALUE_UNKNOWN;
			else if (lzma_vli_add(iter->uncompressed_offset,
					iter_index->uncompressed_size))
				return LZMA_DATA_ERROR;
		}

		if (iter_index->next != NULL) {
			// The next Record has already been allocated.
			iter->internal[ITER_INDEX] = iter_index->next;
			iter->total_size = iter_index->total_size;
			iter->uncompressed_size
					= iter_index->uncompressed_size;
			return LZMA_OK;
		}
	}

	// Don't add new Records to a final Index.
	if (iter_info->index.is_final)
		return LZMA_DATA_ERROR;

	// Allocate and initialize a new Index Record.
	lzma_index *i = lzma_alloc(sizeof(lzma_index), allocator);
	if (i == NULL)
		return LZMA_MEM_ERROR;

	i->total_size = LZMA_VLI_VALUE_UNKNOWN;
	i->uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	i->next = NULL;

	iter->total_size = LZMA_VLI_VALUE_UNKNOWN;
	iter->uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;

	// Decide where to put the new Index Record.
	if (iter_info->index.head == NULL)
		iter_info->index.head = i;

	if (iter_index != NULL)
		iter_index->next = i;

	iter->internal[ITER_INDEX] = i;

	++iter_info->index.record_count;
	++iter_info->index.incomplete_count;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_info_iter_set(lzma_info_iter *iter,
		lzma_vli total_size, lzma_vli uncompressed_size)
{
	// FIXME debug remove
	lzma_info *info = iter_info;
	(void)info;

	if (iter_index == NULL || !lzma_vli_is_valid(total_size)
			|| !lzma_vli_is_valid(uncompressed_size))
		return LZMA_PROG_ERROR;

	const bool was_incomplete = iter_index->total_size
				== LZMA_VLI_VALUE_UNKNOWN
			|| iter_index->uncompressed_size
				== LZMA_VLI_VALUE_UNKNOWN;

	if (total_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (iter_index->total_size == LZMA_VLI_VALUE_UNKNOWN) {
			iter_index->total_size = total_size;

			if (lzma_vli_add(iter_info->index.total_size,
						total_size)
					|| iter_info->index.total_size
						> iter_info->known.total_size)
				return LZMA_DATA_ERROR;

		} else if (iter_index->total_size != total_size) {
			return LZMA_DATA_ERROR;
		}
	}

	if (uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (iter_index->uncompressed_size == LZMA_VLI_VALUE_UNKNOWN) {
			iter_index->uncompressed_size = uncompressed_size;

			if (lzma_vli_add(iter_info->index.uncompressed_size,
						uncompressed_size)
					|| iter_info->index.uncompressed_size
					> iter_info->known.uncompressed_size)
				return LZMA_DATA_ERROR;

		} else if (iter_index->uncompressed_size
				!= uncompressed_size) {
			return LZMA_DATA_ERROR;
		}
	}

	// Check if the new information we got managed to finish this
	// Index Record. If so, update the count of incomplete Index Records.
	if (was_incomplete && iter_index->total_size
				!= LZMA_VLI_VALUE_UNKNOWN
			&& iter_index->uncompressed_size
				!= LZMA_VLI_VALUE_UNKNOWN) {
		assert(iter_info->index.incomplete_count > 0);
		--iter_info->index.incomplete_count;
	}

	// Make sure that the known sizes are now available in *iter.
	iter->total_size = iter_index->total_size;
	iter->uncompressed_size = iter_index->uncompressed_size;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_info_index_finish(lzma_info *info)
{
	if (info->index.record_count == 0 || info->index.incomplete_count > 0
			|| lzma_info_size_set(info, LZMA_INFO_TOTAL,
				info->index.total_size)
			|| lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
				info->index.uncompressed_size))
		return LZMA_DATA_ERROR;

	info->index.is_final = true;

	return LZMA_OK;
}


//////////////
// Locating //
//////////////

extern LZMA_API lzma_vli
lzma_info_metadata_locate(const lzma_info *info, lzma_bool is_header_metadata)
{
	bool error = false;
	lzma_vli size = 0;

	if (info->known.header_metadata_size == LZMA_VLI_VALUE_UNKNOWN) {
		// We don't know if Header Metadata Block is present, thus
		// we cannot locate it either.
		//
		// Well, you could say that just assume that it is present.
		// I'm not sure if this is useful. But it can be useful to
		// be able to use this function and get LZMA_VLI_VALUE_UNKNOWN
		// to detect that Header Metadata Block wasn't present.
		error = true;
	} else if (is_header_metadata) {
		error = lzma_vli_sum(size, info->stream_start_offset,
				LZMA_STREAM_HEADER_SIZE);
	} else if (!info->index.is_final) {
		// Since we don't know if we have all the Index Records yet,
		// we cannot know where the Footer Metadata Block is.
		error = true;
	} else {
		error = lzma_vli_sum4(size, info->stream_start_offset,
				LZMA_STREAM_HEADER_SIZE,
				info->known.header_metadata_size,
				info->known.total_size);
	}

	return error ? LZMA_VLI_VALUE_UNKNOWN : size;
}


extern LZMA_API uint32_t
lzma_info_metadata_alignment_get(
		const lzma_info *info, lzma_bool is_header_metadata)
{
	uint32_t alignment;

	if (is_header_metadata) {
		alignment = info->stream_start_offset
				+ LZMA_STREAM_HEADER_SIZE;
	} else {
		alignment = info->stream_start_offset + LZMA_STREAM_HEADER_SIZE
				+ info->known.header_metadata_size
				+ info->known.total_size;
	}

	return alignment;
}


extern LZMA_API lzma_ret
lzma_info_iter_locate(lzma_info_iter *iter, lzma_allocator *allocator,
		lzma_vli uncompressed_offset, lzma_bool allow_alloc)
{
	if (iter == NULL || uncompressed_offset > LZMA_VLI_VALUE_MAX)
		return LZMA_PROG_ERROR;

	// Quick check in case Index is final.
	if (iter_info->index.is_final) {
		assert(iter_info->known.uncompressed_size
				== iter_info->index.uncompressed_size);
		if (uncompressed_offset >= iter_info->index.uncompressed_size)
			return LZMA_DATA_ERROR;
	}

	// TODO: Optimize so that it uses existing info from *iter when
	// seeking forward.

	// Initialize *iter
	if (iter_info->known.header_metadata_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (lzma_vli_sum3(iter->stream_offset,
				iter_info->stream_start_offset,
				LZMA_STREAM_HEADER_SIZE,
				iter_info->known.header_metadata_size))
			return LZMA_PROG_ERROR;
	} else {
		// We don't know the Size of Header Metadata Block, thus
		// we cannot calculate the Stream offset either.
		iter->stream_offset = LZMA_VLI_VALUE_UNKNOWN;
	}

	iter->uncompressed_offset = 0;

	// If we have no Index Records, it's obvious that we need to
	// add a new one.
	if (iter_info->index.head == NULL) {
		assert(!iter_info->index.is_final);
		if (!allow_alloc)
			return LZMA_DATA_ERROR;

		return lzma_info_iter_next(iter, allocator);
	}

	// Locate an appropriate Index Record.
	lzma_index *i = iter_info->index.head;
	while (true) {
		// - If Uncompressed Size in the Record is unknown,
		//   we have no chance to search further.
		// - If the next Record would go past the requested offset,
		//   we have found our target Data Block.
		if (i->uncompressed_size == LZMA_VLI_VALUE_UNKNOWN
				|| iter->uncompressed_offset
				+ i->uncompressed_size > uncompressed_offset) {
			iter->total_size = i->total_size;
			iter->uncompressed_size = i->uncompressed_size;
			iter->internal[ITER_INDEX] = i;
			return LZMA_OK;
		}

		// Update the stream offset. It may be unknown if we didn't
		// know the size of Header Metadata Block.
		if (iter->stream_offset != LZMA_VLI_VALUE_UNKNOWN)
			if (lzma_vli_add(iter->stream_offset, i->total_size))
				return LZMA_PROG_ERROR;

		// Update the uncompressed offset. This cannot overflow since
		// the Index is known to be valid.
		iter->uncompressed_offset += i->uncompressed_size;

		// Move to the next Block.
		if (i->next == NULL) {
			assert(!iter_info->index.is_final);
			if (!allow_alloc)
				return LZMA_DATA_ERROR;

			iter->internal[ITER_INDEX] = i;
			return lzma_info_iter_next(iter, allocator);
		}

		i = i->next;
	}
}
