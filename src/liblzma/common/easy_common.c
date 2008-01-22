///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_common.c
/// \brief      Shared stuff for easy encoder initialization functions
//
//  Copyright (C) 2008 Lasse Collin
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

#include "easy_common.h"


extern bool
lzma_easy_set_filters(lzma_options_filter *filters, uint32_t level)
{
	bool error = false;

	if (level == 0) {
		filters[0].id = LZMA_VLI_VALUE_UNKNOWN;

#ifdef HAVE_FILTER_LZMA
	} else if (level <= 9) {
		filters[0].id = LZMA_FILTER_LZMA;
		filters[0].options = (void *)(&lzma_preset_lzma[level - 1]);
		filters[1].id = LZMA_VLI_VALUE_UNKNOWN;
#endif

	} else {
		error = true;
	}

	return error;
}


extern LZMA_API uint32_t
lzma_easy_memory_usage(lzma_easy_level level)
{
	lzma_options_filter filters[8];
	if (lzma_easy_set_filters(filters, level))
		return UINT32_MAX;

	return lzma_memory_usage(filters, true);
}
