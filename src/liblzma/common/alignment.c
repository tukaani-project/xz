///////////////////////////////////////////////////////////////////////////////
//
/// \file       alignment.c
/// \brief      Calculates preferred alignments of different filters
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


extern LZMA_API uint32_t
lzma_alignment_input(const lzma_options_filter *filters, uint32_t guess)
{
	for (size_t i = 0; filters[i].id != LZMA_VLI_VALUE_UNKNOWN; ++i) {
		switch (filters[i].id) {
		case LZMA_FILTER_DELTA:
			// The same as the input, check the next filter.
			continue;

		case LZMA_FILTER_SUBBLOCK:
			if (filters[i].options == NULL)
				return LZMA_SUBBLOCK_ALIGNMENT_DEFAULT;
			else
				return ((const lzma_options_subblock *)(
					filters[i].options))->alignment;

		case LZMA_FILTER_X86:
			return 1;

		case LZMA_FILTER_ARMTHUMB:
			return 2;

		case LZMA_FILTER_POWERPC:
		case LZMA_FILTER_ARM:
		case LZMA_FILTER_SPARC:
			return 4;

		case LZMA_FILTER_IA64:
			return 16;

		case LZMA_FILTER_LZMA: {
			const lzma_options_lzma *lzma = filters[i].options;
			return 1 << MAX(lzma->pos_bits,
					lzma->literal_pos_bits);
		}

		default:
			return UINT32_MAX;
		}
	}

	return guess;
}


extern LZMA_API uint32_t
lzma_alignment_output(const lzma_options_filter *filters, uint32_t guess)
{
	if (filters[0].id == LZMA_VLI_VALUE_UNKNOWN)
		return UINT32_MAX;

	// Find the last filter in the chain.
	size_t i = 0;
	while (filters[i + 1].id != LZMA_VLI_VALUE_UNKNOWN)
		++i;

	do {
		switch (filters[i].id) {
		case LZMA_FILTER_DELTA:
			// It's the same as the input alignment, so
			// check the next filter.
			continue;

		case LZMA_FILTER_SUBBLOCK:
			if (filters[i].options == NULL)
				return LZMA_SUBBLOCK_ALIGNMENT_DEFAULT;
			else
				return ((const lzma_options_subblock *)(
					filters[i].options))->alignment;

		case LZMA_FILTER_X86:
		case LZMA_FILTER_LZMA:
			return 1;

		case LZMA_FILTER_ARMTHUMB:
			return 2;

		case LZMA_FILTER_POWERPC:
		case LZMA_FILTER_ARM:
		case LZMA_FILTER_SPARC:
			return 4;

		case LZMA_FILTER_IA64:
			return 16;

		default:
			return UINT32_MAX;
		}
	} while (i-- != 0);

	// If we get here, we have the same alignment as the input data.
	return guess;
}
