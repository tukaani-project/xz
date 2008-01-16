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


/// Rounds upwards to the next multiple of 2 * sizeof(void*).
/// malloc() tends to align allocations this way.
#define malloc_ceil(num) my_ceil(num, 2 * sizeof(void *))


typedef struct lzma_memlimit_list_s lzma_memlimit_list;
struct lzma_memlimit_list_s {
	lzma_memlimit_list *next;
	void *ptr;
	size_t size;
};


struct lzma_memlimit_s {
	size_t used;
	size_t limit;
	lzma_memlimit_list *list;
};


extern LZMA_API lzma_memlimit *
lzma_memlimit_create(size_t limit)
{
	if (limit < sizeof(lzma_memlimit))
		return NULL;

	lzma_memlimit *mem = malloc(sizeof(lzma_memlimit));

	if (mem != NULL) {
		mem->used = sizeof(lzma_memlimit);
		mem->limit = limit;
		mem->list = NULL;
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
	// TODO: We should add some rough estimate how much malloc() needs
	// for its internal structures.
	const size_t total_size = malloc_ceil(size)
			+ malloc_ceil(sizeof(lzma_memlimit_list));

	// Integer overflow protection
	if (SIZE_MAX - size <= total_size)
		return NULL;

	if (mem->limit < mem->used || mem->limit - mem->used < total_size)
		return NULL;

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
