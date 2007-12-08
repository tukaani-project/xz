///////////////////////////////////////////////////////////////////////////////
//
/// \file       next_coder.c
/// \brief      Initializing and freeing the next coder in the chain
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

extern lzma_ret
lzma_next_filter_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	lzma_ret ret = LZMA_OK;

	// Free the existing coder if it is different than the current one.
	if ((uintptr_t)(filters[0].init) != next->init)
		lzma_next_coder_end(next, allocator);

	if (filters[0].init != NULL) {
		// Initialize the new coder.
		ret = filters[0].init(next, allocator, filters);

		// Set the init function pointer if initialization was
		// successful. next->code and next->end are set by the
		// initialization function itself.
		if (ret == LZMA_OK) {
			next->init = (uintptr_t)(filters[0].init);
			assert(next->code != NULL);
			assert(next->end != NULL);
		} else {
			lzma_next_coder_end(next, allocator);
		}
	}

	return ret;
}


extern void
lzma_next_coder_end(lzma_next_coder *next, lzma_allocator *allocator)
{
	if (next != NULL) {
		if (next->end != NULL)
			next->end(next->coder, allocator);

		// Reset the variables so the we don't accidentally think
		// that it is an already initialized coder.
		*next = LZMA_NEXT_CODER_INIT;
	}

	return;
}
