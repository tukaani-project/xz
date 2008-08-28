///////////////////////////////////////////////////////////////////////////////
//
/// \file       memusage.c
/// \brief      Calculates memory usage using lzma_memory_usage()
///
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

#include "sysdefs.h"
#include <stdio.h>

int
main(void)
{
	lzma_init();

	lzma_options_lzma lzma = {
		.dictionary_size = (1 << 27) + (1 << 26),
		.literal_context_bits = 3,
		.literal_pos_bits = 0,
		.pos_bits = 2,
		.preset_dictionary = NULL,
		.preset_dictionary_size = 0,
		.mode = LZMA_MODE_NORMAL,
		.fast_bytes = 48,
		.match_finder = LZMA_MF_BT4,
		.match_finder_cycles = 0,
	};

/*
	lzma_options_filter filters[] = {
		{ LZMA_FILTER_LZMA,
			(lzma_options_lzma *)&lzma_preset_lzma[6 - 1] },
		{ UINT64_MAX, NULL }
	};
*/
	lzma_filter filters[] = {
		{ LZMA_FILTER_LZMA, &lzma },
		{ UINT64_MAX, NULL }
	};

	printf("Encoder: %10" PRIu64 " B\n", lzma_memusage_encoder(filters));
	printf("Decoder: %10" PRIu64 " B\n", lzma_memusage_decoder(filters));

	return 0;
}
