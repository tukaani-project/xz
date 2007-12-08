///////////////////////////////////////////////////////////////////////////////
//
/// \file       allocator.c
/// \brief      Allocating and freeing memory
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

#undef lzma_free

extern void * lzma_attribute((malloc))
lzma_alloc(size_t size, lzma_allocator *allocator)
{
	// Some malloc() variants return NULL if called with size == 0.
	if (size == 0)
		size = 1;

	void *ptr;

	if (allocator != NULL && allocator->alloc != NULL)
		ptr = allocator->alloc(allocator->opaque, 1, size);
	else
		ptr = malloc(size);

#if !defined(NDEBUG) && defined(HAVE_MEMSET)
	// This helps to catch some stupid mistakes.
	if (ptr != NULL)
		memset(ptr, 0xFD, size);
#endif

	return ptr;
}


extern void
lzma_free(void *ptr, lzma_allocator *allocator)
{
	if (allocator != NULL && allocator->free != NULL)
		allocator->free(allocator->opaque, ptr);
	else
		free(ptr);

	return;
}
