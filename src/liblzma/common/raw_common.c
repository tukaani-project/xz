///////////////////////////////////////////////////////////////////////////////
//
/// \file       raw_common.c
/// \brief      Stuff shared between raw encoder and raw decoder
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

#include "raw_common.h"


static lzma_ret
validate_options(const lzma_options_filter *options, size_t *count)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	// Number of non-last filters that may change the size of the data
	// significantly (that is, more than 1-2 % or so).
	size_t change = 0;

	// True if the last filter in the given chain is actually usable as
	// the last filter. Only filters that support embedding End of Payload
	// Marker can be used as the last filter in the chain.
	bool last_ok = false;

	size_t i;
	for (i = 0; options[i].id != LZMA_VLI_VALUE_UNKNOWN; ++i) {
		switch (options[i].id) {
		// Not #ifdeffing these for simplicity.
		case LZMA_FILTER_X86:
		case LZMA_FILTER_POWERPC:
		case LZMA_FILTER_IA64:
		case LZMA_FILTER_ARM:
		case LZMA_FILTER_ARMTHUMB:
		case LZMA_FILTER_SPARC:
		case LZMA_FILTER_DELTA:
			// These don't change the size of the data and cannot
			// be used as the last filter in the chain.
			last_ok = false;
			break;

#ifdef HAVE_FILTER_SUBBLOCK
		case LZMA_FILTER_SUBBLOCK:
			last_ok = true;
			++change;
			break;
#endif

#ifdef HAVE_FILTER_LZMA
		case LZMA_FILTER_LZMA:
			last_ok = true;
			break;
#endif

		default:
			return LZMA_HEADER_ERROR;
		}
	}

	// There must be 1-4 filters and the last filter must be usable as
	// the last filter in the chain.
	if (i == 0 || i > 4 || !last_ok)
		return LZMA_HEADER_ERROR;

	// At maximum of two non-last filters are allowed to change the
	// size of the data.
	if (change > 2)
		return LZMA_HEADER_ERROR;

	*count = i;
	return LZMA_OK;
}


extern lzma_ret
lzma_raw_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options,
		lzma_init_function (*get_function)(lzma_vli id),
		bool is_encoder)
{
	// Do some basic validation and get the number of filters.
	size_t count;
	return_if_error(validate_options(options, &count));

	// Set the filter functions and copy the options pointer.
	lzma_filter_info filters[count + 1];
	if (is_encoder) {
		for (size_t i = 0; i < count; ++i) {
			// The order of the filters is reversed in the
			// encoder. It allows more efficient handling
			// of the uncompressed data.
			const size_t j = count - i - 1;

			filters[j].init = get_function(options[i].id);
			if (filters[j].init == NULL)
				return LZMA_HEADER_ERROR;

			filters[j].options = options[i].options;
		}
	} else {
		for (size_t i = 0; i < count; ++i) {
			filters[i].init = get_function(options[i].id);
			if (filters[i].init == NULL)
				return LZMA_HEADER_ERROR;

			filters[i].options = options[i].options;
		}
	}

	// Terminate the array.
	filters[count].init = NULL;

	// Initialize the filters.
	return lzma_next_filter_init(next, allocator, filters);
}
