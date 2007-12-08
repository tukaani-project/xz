///////////////////////////////////////////////////////////////////////////////
//
/// \file       alloc.h
/// \brief      Memory allocation functions
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ALLOC_H
#define ALLOC_H

#include "private.h"


/// Safe malloc() that never returns NULL.
extern void *xmalloc(size_t size);

/// Safe realloc() that never returns NULL.
extern void *xrealloc(void *ptr, size_t size);

/// Safe strdup() that never returns NULL.
extern char *xstrdup(const char *src);

/// xrealloc()s *dest to the size needed by src, and copies src to *dest.
extern void xstrcpy(char **dest, const char *src);

/// Function for lzma_allocator.alloc. This uses xmalloc().
extern void *allocator(void *opaque lzma_attribute((unused)),
		size_t nmemb lzma_attribute((unused)), size_t size);

#endif
