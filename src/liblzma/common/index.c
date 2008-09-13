///////////////////////////////////////////////////////////////////////////////
//
/// \file       index.c
/// \brief      Handling of Index
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

#include "index.h"


/// Number of Records to allocate at once.
#define INDEX_GROUP_SIZE 256


typedef struct lzma_index_group_s lzma_index_group;
struct lzma_index_group_s {
	/// Next group
	lzma_index_group *prev;

	/// Previous group
	lzma_index_group *next;

	/// Index of the last Record in this group
	size_t last;

	/// Total Size fields as cumulative sum relative to the beginning
	/// of the group. The total size of the group is total_sums[last].
	lzma_vli total_sums[INDEX_GROUP_SIZE];

	/// Uncompressed Size fields as cumulative sum relative to the
	/// beginning of the group. The uncompressed size of the group is
	/// uncompressed_sums[last].
	lzma_vli uncompressed_sums[INDEX_GROUP_SIZE];

	/// True if the Record is padding
	bool paddings[INDEX_GROUP_SIZE];
};


struct lzma_index_s {
	/// Total size of the Blocks and padding
	lzma_vli total_size;

	/// Uncompressed size of the Stream
	lzma_vli uncompressed_size;

	/// Number of non-padding records. This is needed by Index encoder.
	lzma_vli count;

	/// Size of the List of Records field; this is updated every time
	/// a new non-padding Record is added.
	lzma_vli index_list_size;

	/// This is zero if no Indexes have been combined with
	/// lzma_index_cat(). With combined Indexes, this contains the sizes
	/// of all but latest the Streams, including possible Stream Padding
	/// fields.
	lzma_vli padding_size;

	/// First group of Records
	lzma_index_group *head;

	/// Last group of Records
	lzma_index_group *tail;

	/// Tracking the read position
	struct {
		/// Group where the current read position is.
		lzma_index_group *group;

		/// The most recently read record in *group
		lzma_vli record;

		/// Uncompressed offset of the beginning of *group relative
		/// to the beginning of the Stream
		lzma_vli uncompressed_offset;

		/// Compressed offset of the beginning of *group relative
		/// to the beginning of the Stream
		lzma_vli stream_offset;
	} current;

	/// Information about earlier Indexes when multiple Indexes have
	/// been combined.
	struct {
		/// Sum of the Record counts of the all but the last Stream.
		lzma_vli count;

		/// Sum of the List of Records fields of all but the last
		/// Stream. This is needed when a new Index is concatenated
		/// to this lzma_index structure.
		lzma_vli index_list_size;
	} old;
};


static void
free_index_list(lzma_index *i, lzma_allocator *allocator)
{
	lzma_index_group *g = i->head;

	while (g != NULL) {
		lzma_index_group *tmp = g->next;
		lzma_free(g, allocator);
		g = tmp;
	}

	return;
}


extern LZMA_API lzma_index *
lzma_index_init(lzma_index *i, lzma_allocator *allocator)
{
	if (i == NULL) {
		i = lzma_alloc(sizeof(lzma_index), allocator);
		if (i == NULL)
			return NULL;
	} else {
		free_index_list(i, allocator);
	}

	i->total_size = 0;
	i->uncompressed_size = 0;
	i->count = 0;
	i->index_list_size = 0;
	i->padding_size = 0;
	i->head = NULL;
	i->tail = NULL;
	i->current.group = NULL;
	i->old.count = 0;
	i->old.index_list_size = 0;

	return i;
}


extern LZMA_API void
lzma_index_end(lzma_index *i, lzma_allocator *allocator)
{
	if (i != NULL) {
		free_index_list(i, allocator);
		lzma_free(i, allocator);
	}

	return;
}


extern LZMA_API lzma_vli
lzma_index_count(const lzma_index *i)
{
	return i->count;
}


extern LZMA_API lzma_vli
lzma_index_size(const lzma_index *i)
{
	return index_size(i->count, i->index_list_size);
}


extern LZMA_API lzma_vli
lzma_index_total_size(const lzma_index *i)
{
	return i->total_size;
}


extern LZMA_API lzma_vli
lzma_index_stream_size(const lzma_index *i)
{
	// Stream Header + Blocks + Index + Stream Footer
	return LZMA_STREAM_HEADER_SIZE + i->total_size
			+ index_size(i->count, i->index_list_size)
			+ LZMA_STREAM_HEADER_SIZE;
}


extern LZMA_API lzma_vli
lzma_index_file_size(const lzma_index *i)
{
	// If multiple Streams are concatenated, the Stream Header, Index,
	// and Stream Footer fields of all but the last Stream are already
	// included in padding_size. Thus, we need to calculate only the
	// size of the last Index, not all Indexes.
	return i->total_size + i->padding_size
			+ index_size(i->count - i->old.count,
				i->index_list_size - i->old.index_list_size)
			+ LZMA_STREAM_HEADER_SIZE * 2;
}


extern LZMA_API lzma_vli
lzma_index_uncompressed_size(const lzma_index *i)
{
	return i->uncompressed_size;
}


extern uint32_t
lzma_index_padding_size(const lzma_index *i)
{
	return (LZMA_VLI_C(4)
		- index_size_unpadded(i->count, i->index_list_size)) & 3;
}


/// Helper function for index_append()
static lzma_ret
index_append_real(lzma_index *i, lzma_allocator *allocator,
		lzma_vli total_size, lzma_vli uncompressed_size,
		bool is_padding)
{
	// Add the new record.
	if (i->tail == NULL || i->tail->last == INDEX_GROUP_SIZE - 1) {
		// Allocate a new group.
		lzma_index_group *g = lzma_alloc(sizeof(lzma_index_group),
				allocator);
		if (g == NULL)
			return LZMA_MEM_ERROR;

		// Initialize the group and set its first record.
		g->prev = i->tail;
		g->next = NULL;
		g->last = 0;
		g->total_sums[0] = total_size;
		g->uncompressed_sums[0] = uncompressed_size;
		g->paddings[0] = is_padding;

		// If this is the first group, make it the head.
		if (i->head == NULL)
			i->head = g;
		else
			i->tail->next = g;

		// Make it the new tail.
		i->tail = g;

	} else {
		// i->tail has space left for at least one record.
		i->tail->total_sums[i->tail->last + 1]
				= i->tail->total_sums[i->tail->last]
					+ total_size;
		i->tail->uncompressed_sums[i->tail->last + 1]
				= i->tail->uncompressed_sums[i->tail->last]
					+ uncompressed_size;
		i->tail->paddings[i->tail->last + 1] = is_padding;
		++i->tail->last;
	}

	return LZMA_OK;
}


static lzma_ret
index_append(lzma_index *i, lzma_allocator *allocator, lzma_vli total_size,
		lzma_vli uncompressed_size, bool is_padding)
{
	if (total_size > LZMA_VLI_MAX
			|| uncompressed_size > LZMA_VLI_MAX)
		return LZMA_DATA_ERROR;

	// This looks a bit ugly. We want to first validate that the Index
	// and Stream stay in valid limits after adding this Record. After
	// validating, we may need to allocate a new lzma_index_group (it's
	// slightly more correct to validate before allocating, YMMV).
	lzma_ret ret;

	if (is_padding) {
		assert(uncompressed_size == 0);

		// First update the info so we can validate it.
		i->padding_size += total_size;

		if (i->padding_size > LZMA_VLI_MAX
				|| lzma_index_file_size(i) > LZMA_VLI_MAX)
			ret = LZMA_DATA_ERROR; // Would grow past the limits.
		else
			ret = index_append_real(i, allocator,
					total_size, uncompressed_size, true);

		// If something went wrong, undo the updated value.
		if (ret != LZMA_OK)
			i->padding_size -= total_size;

	} else {
		// First update the overall info so we can validate it.
		const lzma_vli index_list_size_add
				= lzma_vli_size(total_size / 4 - 1)
				+ lzma_vli_size(uncompressed_size);

		i->total_size += total_size;
		i->uncompressed_size += uncompressed_size;
		++i->count;
		i->index_list_size += index_list_size_add;

		if (i->total_size > LZMA_VLI_MAX
				|| i->uncompressed_size > LZMA_VLI_MAX
				|| lzma_index_size(i) > LZMA_BACKWARD_SIZE_MAX
				|| lzma_index_file_size(i) > LZMA_VLI_MAX)
			ret = LZMA_DATA_ERROR; // Would grow past the limits.
		else
			ret = index_append_real(i, allocator,
					total_size, uncompressed_size, false);

		if (ret != LZMA_OK) {
			// Something went wrong. Undo the updates.
			i->total_size -= total_size;
			i->uncompressed_size -= uncompressed_size;
			--i->count;
			i->index_list_size -= index_list_size_add;
		}
	}

	return ret;
}


extern LZMA_API lzma_ret
lzma_index_append(lzma_index *i, lzma_allocator *allocator,
		lzma_vli total_size, lzma_vli uncompressed_size)
{
	return index_append(i, allocator,
			total_size, uncompressed_size, false);
}


/// Initialize i->current to point to the first Record.
static bool
init_current(lzma_index *i)
{
	if (i->head == NULL) {
		assert(i->count == 0);
		return true;
	}

	assert(i->count > 0);

	i->current.group = i->head;
	i->current.record = 0;
	i->current.stream_offset = LZMA_STREAM_HEADER_SIZE;
	i->current.uncompressed_offset = 0;

	return false;
}


/// Go backward to the previous group.
static void
previous_group(lzma_index *i)
{
	assert(i->current.group->prev != NULL);

	// Go to the previous group first.
	i->current.group = i->current.group->prev;
	i->current.record = i->current.group->last;

	// Then update the offsets.
	i->current.stream_offset -= i->current.group
			->total_sums[i->current.group->last];
	i->current.uncompressed_offset -= i->current.group
			->uncompressed_sums[i->current.group->last];

	return;
}


/// Go forward to the next group.
static void
next_group(lzma_index *i)
{
	assert(i->current.group->next != NULL);

	// Update the offsets first.
	i->current.stream_offset += i->current.group
			->total_sums[i->current.group->last];
	i->current.uncompressed_offset += i->current.group
			->uncompressed_sums[i->current.group->last];

	// Then go to the next group.
	i->current.record = 0;
	i->current.group = i->current.group->next;

	return;
}


/// Set *info from i->current.
static void
set_info(const lzma_index *i, lzma_index_record *info)
{
	info->total_size = i->current.group->total_sums[i->current.record];
	info->uncompressed_size = i->current.group->uncompressed_sums[
			i->current.record];

	info->stream_offset = i->current.stream_offset;
	info->uncompressed_offset = i->current.uncompressed_offset;

	// If it's not the first Record in this group, we need to do some
	// adjustements.
	if (i->current.record > 0) {
		// _sums[] are cumulative, thus we need to substract the
		// _previous _sums[] to get the sizes of this Record.
		info->total_size -= i->current.group
				->total_sums[i->current.record - 1];
		info->uncompressed_size -= i->current.group
				->uncompressed_sums[i->current.record - 1];

		// i->current.{total,uncompressed}_offsets have the offset
		// of the beginning of the group, thus we need to add the
		// appropriate amount to get the offsetes of this Record.
		info->stream_offset += i->current.group
				->total_sums[i->current.record - 1];
		info->uncompressed_offset += i->current.group
				->uncompressed_sums[i->current.record - 1];
	}

	return;
}


extern LZMA_API lzma_bool
lzma_index_read(lzma_index *i, lzma_index_record *info)
{
	if (i->current.group == NULL) {
		// We are at the beginning of the Record list. Set up
		// i->current point at the first Record. Return if there
		// are no Records.
		if (init_current(i))
			return true;
	} else do {
		// Try to go the next Record.
		if (i->current.record < i->current.group->last)
			++i->current.record;
		else if (i->current.group->next == NULL)
			return true;
		else
			next_group(i);
	} while (i->current.group->paddings[i->current.record]);

	// We found a new Record. Set the information to *info.
	set_info(i, info);

	return false;
}


extern LZMA_API void
lzma_index_rewind(lzma_index *i)
{
	i->current.group = NULL;
	return;
}


extern LZMA_API lzma_bool
lzma_index_locate(lzma_index *i, lzma_index_record *info, lzma_vli target)
{
	// Check if it is possible to fullfill the request.
	if (target >= i->uncompressed_size)
		return true;

	// Now we know that we will have an answer. Initialize the current
	// read position if needed.
	if (i->current.group == NULL && init_current(i))
		return true;

	// Locate the group where the wanted Block is. First search forward.
	while (i->current.uncompressed_offset <= target) {
		// If the first uncompressed byte of the next group is past
		// the target offset, it has to be this or an earlier group.
		if (i->current.uncompressed_offset + i->current.group
				->uncompressed_sums[i->current.group->last]
				> target)
			break;

		// Go forward to the next group.
		next_group(i);
	}

	// Then search backward.
	while (i->current.uncompressed_offset > target)
		previous_group(i);

	// Now the target Block is somewhere in i->current.group. Offsets
	// in groups are relative to the beginning of the group, thus
	// we must adjust the target before starting the search loop.
	assert(target >= i->current.uncompressed_offset);
	target -= i->current.uncompressed_offset;

	// Use binary search to locate the exact Record. It is the first
	// Record whose uncompressed_sums[] value is greater than target.
	// This is because we want the rightmost Record that fullfills the
	// search criterion. It is possible that there are empty Blocks or
	// padding, we don't want to return them.
	size_t left = 0;
	size_t right = i->current.group->last;

	while (left < right) {
		const size_t pos = left + (right - left) / 2;
		if (i->current.group->uncompressed_sums[pos] <= target)
			left = pos + 1;
		else
			right = pos;
	}

	i->current.record = left;

#ifndef NDEBUG
	// The found Record must not be padding or have zero uncompressed size.
	assert(!i->current.group->paddings[i->current.record]);

	if (i->current.record == 0)
		assert(i->current.group->uncompressed_sums[0] > 0);
	else
		assert(i->current.group->uncompressed_sums[i->current.record]
				- i->current.group->uncompressed_sums[
					i->current.record - 1] > 0);
#endif

	set_info(i, info);

	return false;
}


extern LZMA_API lzma_ret
lzma_index_cat(lzma_index *restrict dest, lzma_index *restrict src,
		lzma_allocator *allocator, lzma_vli padding)
{
	if (dest == NULL || src == NULL || dest == src
			|| padding > LZMA_VLI_MAX)
		return LZMA_PROG_ERROR;

	// Check that the combined size of the Indexes stays within limits.
	{
		const lzma_vli dest_size = lzma_index_file_size(dest);
		const lzma_vli src_size = lzma_index_file_size(src);
		if (dest_size + src_size > LZMA_VLI_UNKNOWN
				|| dest_size + src_size + padding
					> LZMA_VLI_UNKNOWN)
			return LZMA_DATA_ERROR;
	}

	// Add a padding Record to take into account the size of
	// Index + Stream Footer + Stream Padding + Stream Header.
	//
	// NOTE: This cannot overflow, because Index Size is always
	// far smaller than LZMA_VLI_MAX, and adding two VLIs
	// (Index Size and padding) doesn't overflow. It may become
	// an invalid VLI if padding is huge, but that is caught by
	// index_append().
	padding += index_size(dest->count - dest->old.count,
				dest->index_list_size
					- dest->old.index_list_size)
			+ LZMA_STREAM_HEADER_SIZE * 2;

	// Add the padding Record.
	return_if_error(index_append(
			dest, allocator, padding, 0, true));

	// Avoid wasting lots of memory if src->head has only a few records
	// that fit into dest->tail. That is, combine two groups if possible.
	//
	// NOTE: We know that dest->tail != NULL since we just appended
	// a padding Record. But we don't know about src->head.
	if (src->head != NULL && src->head->last + 1
			<= INDEX_GROUP_SIZE - dest->tail->last - 1) {
		// Copy the first Record.
		dest->tail->total_sums[dest->tail->last + 1]
			= dest->tail->total_sums[dest->tail->last]
				+ src->head->total_sums[0];

		dest->tail->uncompressed_sums[dest->tail->last + 1]
			= dest->tail->uncompressed_sums[dest->tail->last]
				+ src->head->uncompressed_sums[0];

		dest->tail->paddings[dest->tail->last + 1]
				= src->head->paddings[0];

		++dest->tail->last;

		// Copy the rest.
		for (size_t i = 1; i < src->head->last; ++i) {
			dest->tail->total_sums[dest->tail->last + 1]
				= dest->tail->total_sums[dest->tail->last]
					+ src->head->total_sums[i + 1]
					- src->head->total_sums[i];

			dest->tail->uncompressed_sums[dest->tail->last + 1]
				= dest->tail->uncompressed_sums[
						dest->tail->last]
					+ src->head->uncompressed_sums[i + 1]
					- src->head->uncompressed_sums[i];

			dest->tail->paddings[dest->tail->last + 1]
				= src->head->paddings[i + 1];

			++dest->tail->last;
		}

		// Free the head group of *src. Don't bother updating prev
		// pointers since those won't be used for anything before
		// we deallocate the whole *src structure.
		lzma_index_group *tmp = src->head;
		src->head = src->head->next;
		lzma_free(tmp, allocator);
	}

	// If there are groups left in *src, join them as is. Note that if we
	// are combining already combined Indexes, src->head can be non-NULL
	// even if we just combined the old src->head to dest->tail.
	if (src->head != NULL) {
		src->head->prev = dest->tail;
		dest->tail->next = src->head;
		dest->tail = src->tail;
	}

	// Update information about earlier Indexes. Only the last Index
	// from *src won't be counted in dest->old. The last Index is left
	// open and can be even appended with lzma_index_append().
	dest->old.count = dest->count + src->old.count;
	dest->old.index_list_size
			= dest->index_list_size + src->old.index_list_size;

	// Update overall information.
	dest->total_size += src->total_size;
	dest->uncompressed_size += src->uncompressed_size;
	dest->count += src->count;
	dest->index_list_size += src->index_list_size;
	dest->padding_size += src->padding_size;

	// *src has nothing left but the base structure.
	lzma_free(src, allocator);

	return LZMA_OK;
}


extern LZMA_API lzma_index *
lzma_index_dup(const lzma_index *src, lzma_allocator *allocator)
{
	lzma_index *dest = lzma_alloc(sizeof(lzma_index), allocator);
	if (dest == NULL)
		return NULL;

	// Copy the base structure except the pointers.
	*dest = *src;
	dest->head = NULL;
	dest->tail = NULL;
	dest->current.group = NULL;

	// Copy the Records.
	const lzma_index_group *src_group = src->head;
	while (src_group != NULL) {
		// Allocate a new group.
		lzma_index_group *dest_group = lzma_alloc(
				sizeof(lzma_index_group), allocator);
		if (dest_group == NULL) {
			lzma_index_end(dest, allocator);
			return NULL;
		}

		// Set the pointers.
		dest_group->prev = dest->tail;
		dest_group->next = NULL;

		if (dest->head == NULL)
			dest->head = dest_group;
		else
			dest->tail->next = dest_group;

		dest->tail = dest_group;

		dest_group->last = src_group->last;

		// Copy the arrays so that we don't read uninitialized memory.
		const size_t count = src_group->last + 1;
		memcpy(dest_group->total_sums, src_group->total_sums,
				sizeof(lzma_vli) * count);
		memcpy(dest_group->uncompressed_sums,
				src_group->uncompressed_sums,
				sizeof(lzma_vli) * count);
		memcpy(dest_group->paddings, src_group->paddings,
				sizeof(bool) * count);

		// Copy also the read position.
		if (src_group == src->current.group)
			dest->current.group = dest->tail;

		src_group = src_group->next;
	}

	return dest;
}


extern LZMA_API lzma_bool
lzma_index_equal(const lzma_index *a, const lzma_index *b)
{
	// No point to compare more if the pointers are the same.
	if (a == b)
		return true;

	// Compare the basic properties.
	if (a->total_size != b->total_size
			|| a->uncompressed_size != b->uncompressed_size
			|| a->index_list_size != b->index_list_size
			|| a->count != b->count)
		return false;

	// Compare the Records.
	const lzma_index_group *ag = a->head;
	const lzma_index_group *bg = b->head;
	while (ag != NULL && bg != NULL) {
		const size_t count = ag->last + 1;
		if (ag->last != bg->last
				|| memcmp(ag->total_sums,
					bg->total_sums,
					sizeof(lzma_vli) * count) != 0
				|| memcmp(ag->uncompressed_sums,
					bg->uncompressed_sums,
					sizeof(lzma_vli) * count) != 0
				|| memcmp(ag->paddings, bg->paddings,
					sizeof(bool) * count) != 0)
			return false;

		ag = ag->next;
		bg = bg->next;
	}

	return ag == NULL && bg == NULL;
}
