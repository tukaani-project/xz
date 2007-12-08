///////////////////////////////////////////////////////////////////////////////
//
/// \file       memory_usage.c
/// \brief      Calculate rough amount of memory required by filters
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
#include "lz_encoder.h"
#include "lzma_literal.h"


static uint64_t
get_usage(const lzma_options_filter *filter, bool is_encoder)
{
	uint64_t ret;

	switch (filter->id) {
	case LZMA_FILTER_COPY:
	case LZMA_FILTER_X86:
	case LZMA_FILTER_POWERPC:
	case LZMA_FILTER_IA64:
	case LZMA_FILTER_ARM:
	case LZMA_FILTER_ARMTHUMB:
	case LZMA_FILTER_SPARC:
	case LZMA_FILTER_DELTA:
		// These don't require any significant amount of memory.
		ret = 0;
		break;

	case LZMA_FILTER_SUBBLOCK:
		if (is_encoder) {
			const lzma_options_subblock *options = filter->options;
			ret = options->subblock_data_size;
		} else {
			ret = 0;
		}
		break;

#ifdef HAVE_FILTER_LZMA
	case LZMA_FILTER_LZMA: {
		const lzma_options_lzma *options = filter->options;

		// Literal coder - this can be signficant if both values are
		// big, or if sizeof(probability) is big.
		ret = literal_states(options->literal_context_bits,
				options->literal_pos_bits) * LIT_SIZE
				* sizeof(probability);

		// Dictionary base size
		ret += options->dictionary_size;

		if (is_encoder) {
#	ifdef HAVE_ENCODER
			// This is rough, but should be accurate enough
			// in practice.
			ret += options->dictionary_size / 2;

			uint32_t dummy1;
			uint32_t dummy2;
			uint32_t num_items;
			if (lzma_lz_encoder_hash_properties(
					options->match_finder,
					options->dictionary_size,
					&dummy1, &dummy2, &num_items))
				return UINT64_MAX;

			ret += (uint64_t)(num_items) * sizeof(uint32_t);
#	else
			return UINT64_MAX;
#	endif
		}

		break;
	}
#endif

	default:
		return UINT64_MAX;
	}

	return ret;
}


extern LZMA_API uint32_t
lzma_memory_usage(const lzma_options_filter *filters, lzma_bool is_encoder)
{
	uint64_t usage = 0;

	for (size_t i = 0; filters[i].id != UINT64_MAX; ++i) {
		const uint64_t ret = get_usage(filters + i, is_encoder);
		if (ret == UINT64_MAX)
			return UINT32_MAX;

		usage += ret;
	}

	// Convert to mebibytes with rounding.
	return usage / (1024 * 1024) + (usage % (1024 * 1024) >= 512 ? 1 : 0);
}
