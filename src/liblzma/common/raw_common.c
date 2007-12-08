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


/// \brief      Prepares the filter chain
///
/// Prepares the filter chain by setting uncompressed sizes for each filter,
/// and adding implicit Subblock filter when needed.
///
/// \return     true if error occurred, false on success.
///
static bool
prepare(lzma_vli *id, lzma_vli *uncompressed_size, bool implicit)
{
	bool needs_end_of_input = false;

	switch (id[0]) {
	case LZMA_FILTER_COPY:
	case LZMA_FILTER_X86:
	case LZMA_FILTER_POWERPC:
	case LZMA_FILTER_IA64:
	case LZMA_FILTER_ARM:
	case LZMA_FILTER_ARMTHUMB:
	case LZMA_FILTER_SPARC:
	case LZMA_FILTER_DELTA:
		uncompressed_size[1] = uncompressed_size[0];
		needs_end_of_input = true;
		break;

	case LZMA_FILTER_SUBBLOCK:
	case LZMA_FILTER_LZMA:
		// These change the size of the data unpredictably.
		uncompressed_size[1] = LZMA_VLI_VALUE_UNKNOWN;
		break;

	case LZMA_FILTER_SUBBLOCK_HELPER:
		uncompressed_size[1] = uncompressed_size[0];
		break;

	default:
		// Unknown filter.
		return true;
	}

	// Is this the last filter in the chain?
	if (id[1] == LZMA_VLI_VALUE_UNKNOWN) {
		if (!needs_end_of_input || !implicit || uncompressed_size[0]
				!= LZMA_VLI_VALUE_UNKNOWN)
			return false;

		// Add implicit Subblock filter.
		id[1] = LZMA_FILTER_SUBBLOCK;
		uncompressed_size[1] = LZMA_VLI_VALUE_UNKNOWN;
		id[2] = LZMA_VLI_VALUE_UNKNOWN;
	}

	return prepare(id + 1, uncompressed_size + 1, implicit);
}


extern lzma_ret
lzma_raw_coder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options, lzma_vli uncompressed_size,
		lzma_init_function (*get_function)(lzma_vli id),
		bool allow_implicit, bool is_encoder)
{
	if (options == NULL || !lzma_vli_is_valid(uncompressed_size))
		return LZMA_PROG_ERROR;

	// Count the number of filters in the chain.
	size_t count = 0;
	while (options[count].id != LZMA_VLI_VALUE_UNKNOWN)
		++count;

	// Allocate enough space from the stack for IDs and uncompressed
	// sizes. We need two extra: possible implicit Subblock and end
	// of array indicator.
	lzma_vli ids[count + 2];
	lzma_vli uncompressed_sizes[count + 2];
	bool using_implicit = false;

	uncompressed_sizes[0] = uncompressed_size;

	if (count == 0) {
		if (!allow_implicit)
			return LZMA_PROG_ERROR;

		count = 1;
		using_implicit = true;

		// Special case: no filters were specified, so an implicit
		// Copy or Subblock filter is used.
		if (uncompressed_size == LZMA_VLI_VALUE_UNKNOWN)
			ids[0] = LZMA_FILTER_SUBBLOCK;
		else
			ids[0] = LZMA_FILTER_COPY;

		ids[1] = LZMA_VLI_VALUE_UNKNOWN;

	} else {
		// Prepare the ids[] and uncompressed_sizes[].
		for (size_t i = 0; i < count; ++i)
			ids[i] = options[i].id;

		ids[count] = LZMA_VLI_VALUE_UNKNOWN;

		if (prepare(ids, uncompressed_sizes, allow_implicit))
			return LZMA_HEADER_ERROR;

		// Check if implicit Subblock filter was added.
		if (ids[count] != LZMA_VLI_VALUE_UNKNOWN) {
			assert(ids[count] == LZMA_FILTER_SUBBLOCK);
			++count;
			using_implicit = true;
		}
	}

	// Set the filter functions, and copy uncompressed sizes and options.
	lzma_filter_info filters[count + 1];
	if (is_encoder) {
		for (size_t i = 0; i < count; ++i) {
			// The order of the filters is reversed in the
			// encoder. It allows more efficient handling
			// of the uncompressed data.
			const size_t j = count - i - 1;

			filters[j].init = get_function(ids[i]);
			if (filters[j].init == NULL)
				return LZMA_HEADER_ERROR;

			filters[j].options = options[i].options;
			filters[j].uncompressed_size = uncompressed_sizes[i];
		}

		if (using_implicit)
			filters[0].options = NULL;

	} else {
		for (size_t i = 0; i < count; ++i) {
			filters[i].init = get_function(ids[i]);
			if (filters[i].init == NULL)
				return LZMA_HEADER_ERROR;

			filters[i].options = options[i].options;
			filters[i].uncompressed_size = uncompressed_sizes[i];
		}

		if (using_implicit)
			filters[count - 1].options = NULL;
	}

	// Terminate the array.
	filters[count].init = NULL;

	// Initialize the filters.
	return lzma_next_filter_init(next, allocator, filters);
}
