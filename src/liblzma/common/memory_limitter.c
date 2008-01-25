///////////////////////////////////////////////////////////////////////////////
//
/// \file       memory_limitter.c
/// \brief      Limitting memory usage
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


/// Rounds an unsigned integer upwards to the next multiple.
#define my_ceil(num, multiple) \
	((num) + (((multiple) - ((num) % (multiple))) % (multiple)))


/// Add approximated overhead of malloc() to size and round upwards to the
/// next multiple of 2 * sizeof(size_t). I suppose that most malloc()
/// implementations align small allocations this way, but the overhead
/// varies due to several reasons (free lists, mmap() usage etc.).
///
/// This doesn't need to be exact at all. It's enough to take into account
/// that there is some overhead. That way our memory usage count won't be
/// horribly wrong if we are used to allocate lots of small memory chunks.
#define malloc_ceil(size) \
	my_ceil((size) + 2 * sizeof(void *), 2 * sizeof(size_t))


typedef struct lzma_memlimit_list_s lzma_memlimit_list;
struct lzma_memlimit_list_s {
	lzma_memlimit_list *next;
	void *ptr;
	size_t size;
};


struct lzma_memlimit_s {
	/// List of allocated memory chunks
	lzma_memlimit_list *list;

	/// Number of bytes currently allocated; this includes the memory
	/// needed for the helper structures.
	size_t used;

	/// Memory usage limit
	size_t limit;

	/// Maximum amount of memory that have been or would have been needed.
	/// That is, this is updated also if memory allocation fails, letting
	/// the application check how much memory was tried to be allocated
	/// in total.
	size_t max;

	/// True if lzma_memlimit_alloc() has returned NULL due to memory
	/// usage limit.
	bool limit_reached;
};


extern LZMA_API lzma_memlimit *
lzma_memlimit_create(size_t limit)
{
	const size_t base_size = malloc_ceil(sizeof(lzma_memlimit));

	if (limit < base_size)
		return NULL;

	lzma_memlimit *mem = malloc(sizeof(lzma_memlimit));

	if (mem != NULL) {
		mem->list = NULL;
		mem->used = base_size;
		mem->limit = limit;
		mem->max = base_size;
		mem->limit_reached = false;
	}

	return mem;
}


extern LZMA_API void
lzma_memlimit_set(lzma_memlimit *mem, size_t limit)
{
	mem->limit = limit;
	return;
}


extern LZMA_API size_t
lzma_memlimit_get(const lzma_memlimit *mem)
{
	return mem->limit;
}


extern LZMA_API size_t
lzma_memlimit_used(const lzma_memlimit *mem)
{
	return mem->used;
}


extern LZMA_API size_t
lzma_memlimit_max(lzma_memlimit *mem, lzma_bool clear)
{
	const size_t ret = mem->max;

	if (clear)
		mem->max = mem->used;

	return ret;
}


extern LZMA_API lzma_bool
lzma_memlimit_reached(lzma_memlimit *mem, lzma_bool clear)
{
	const bool ret = mem->limit_reached;

	if (clear)
		mem->limit_reached = false;

	return ret;
}


extern LZMA_API size_t
lzma_memlimit_count(const lzma_memlimit *mem)
{
	// This is slow; we could have a counter in lzma_memlimit
	// for fast version. I expect the primary use of this
	// function to be limited to easy checking of memory leaks,
	// in which this implementation is just fine.
	size_t count = 0;
	const lzma_memlimit_list *record = mem->list;

	while (record != NULL) {
		++count;
		record = record->next;
	}

	return count;
}


extern LZMA_API void
lzma_memlimit_end(lzma_memlimit *mem, lzma_bool free_allocated)
{
	if (mem == NULL)
		return;

	lzma_memlimit_list *record = mem->list;
	while (record != NULL) {
		if (free_allocated)
			free(record->ptr);

		lzma_memlimit_list *tmp = record;
		record = record->next;
		free(tmp);
	}

	free(mem);

	return;
}


extern LZMA_API void *
lzma_memlimit_alloc(lzma_memlimit *mem, size_t nmemb, size_t size)
{
	// While liblzma always sets nmemb to one, do this multiplication
	// to make these functions usable e.g. with zlib and libbzip2.
	// Making sure that this doesn't overflow is up to the application.
	size *= nmemb;

	// Some malloc() implementations return NULL on malloc(0). We like
	// to get a non-NULL value.
	if (size == 0)
		size = 1;

	// Calculate how much memory we are going to allocate in reality.
	const size_t total_size = malloc_ceil(size)
			+ malloc_ceil(sizeof(lzma_memlimit_list));

	// Integer overflow protection for total_size and mem->used.
	if (total_size <= size || SIZE_MAX - total_size < mem->used) {
		mem->max = SIZE_MAX;
		mem->limit_reached = true;
		return NULL;
	}

	// Update the maximum memory requirement counter if needed. This
	// is updated even if memory allocation would fail or limit would
	// be reached.
	if (mem->used + total_size > mem->max)
		mem->max = mem->used + total_size;

	// Check if we would stay in the memory usage limits. We need to
	// check also that the current usage is in the limits, because
	// the application could have decreased the limit between calls
	// to this function.
	if (mem->limit < mem->used || mem->limit - mem->used < total_size) {
		mem->limit_reached = true;
		return NULL;
	}

	// Allocate separate memory chunks for lzma_memlimit_list and the
	// actual requested memory. Optimizing this to use only one
	// allocation is not a good idea, because applications may want to
	// detach lzma_extra structures that have been allocated with
	// lzma_memlimit_alloc().
	lzma_memlimit_list *record = malloc(sizeof(lzma_memlimit_list));
	void *ptr = malloc(size);

	if (record == NULL || ptr == NULL) {
		free(record);
		free(ptr);
		return NULL;
	}

	// Add the new entry to the beginning of the list. This should be
	// more efficient when freeing memory, because usually it is
	// "last allocated, first freed".
	record->next = mem->list;
	record->ptr = ptr;
	record->size = total_size;

	mem->list = record;
	mem->used += total_size;

	return ptr;
}


extern LZMA_API void
lzma_memlimit_detach(lzma_memlimit *mem, void *ptr)
{
	if (ptr == NULL || mem->list == NULL)
		return;

	lzma_memlimit_list *record = mem->list;
	lzma_memlimit_list *prev = NULL;

	while (record->ptr != ptr) {
		prev = record;
		record = record->next;
		if (record == NULL)
			return;
	}

	if (prev != NULL)
		prev->next = record->next;
	else
		mem->list = record->next;

	assert(mem->used >= record->size);
	mem->used -= record->size;

	free(record);

	return;
}


extern LZMA_API void
lzma_memlimit_free(lzma_memlimit *mem, void *ptr)
{
	if (ptr == NULL)
		return;

	lzma_memlimit_detach(mem, ptr);

	free(ptr);

	return;
}
