///////////////////////////////////////////////////////////////////////////////
//
/// \file       extra.c
/// \brief      Handling of Extra in Metadata
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


extern LZMA_API void
lzma_extra_free(lzma_extra *extra, lzma_allocator *allocator)
{
	while (extra != NULL) {
		lzma_extra *tmp = extra->next;
		lzma_free(extra, allocator);
		extra = tmp;
	}

	return;
}
