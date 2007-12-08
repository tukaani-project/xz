///////////////////////////////////////////////////////////////////////////////
//
/// \file       alloc.c
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

#include "private.h"


/// Called when memory allocation fails. Prints and error message and
/// quits the application.
static void lzma_attribute((noreturn))
xerror(void)
{
	errmsg(V_ERROR, "%s", strerror(errno));
	my_exit(ERROR);
}


extern void *
xmalloc(size_t size)
{
	if (size < 1) {
		errno = EINVAL;
		xerror();
	}

	void *p = malloc(size);
	if (p == NULL)
		xerror();

	return p;
}


/*
extern void *
xrealloc(void *ptr, size_t size)
{
	if (size < 1) {
		errno = EINVAL;
		xerror();
	}

	ptr = realloc(ptr, size);
	if (ptr == NULL)
		xerror();

	return ptr;
}
*/


extern char *
xstrdup(const char *src)
{
	if (src == NULL) {
		errno = EINVAL;
		xerror();
	}

	const size_t size = strlen(src) + 1;
	char *dest = malloc(size);
	if (dest == NULL)
		xerror();

	memcpy(dest, src, size);

	return dest;
}


extern void
xstrcpy(char **dest, const char *src)
{
	size_t len = strlen(src) + 1;

	*dest = realloc(*dest, len);
	if (*dest == NULL)
		xerror();

	memcpy(*dest, src, len + 1);

	return;
}


extern void *
allocator(void *opaque lzma_attribute((unused)),
		size_t nmemb lzma_attribute((unused)), size_t size)
{
	return xmalloc(size);
}
