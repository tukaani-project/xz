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
	lzma_options_lzma lzma = {
		.dict_size = (1U << 30) + (1U << 29),
		.lc = 3,
		.lp = 0,
		.pb = 2,
		.preset_dict = NULL,
		.preset_dict_size = 0,
		.mode = LZMA_MODE_NORMAL,
		.nice_len = 48,
		.mf = LZMA_MF_BT4,
		.depth = 0,
	};

/*
	lzma_options_filter filters[] = {
		{ LZMA_FILTER_LZMA1,
			(lzma_options_lzma *)&lzma_preset_lzma[6 - 1] },
		{ UINT64_MAX, NULL }
	};
*/
	lzma_filter filters[] = {
		{ LZMA_FILTER_LZMA1, &lzma },
		{ UINT64_MAX, NULL }
	};

	printf("Encoder: %10" PRIu64 " B\n", lzma_memusage_encoder(filters));
	printf("Decoder: %10" PRIu64 " B\n", lzma_memusage_decoder(filters));

	return 0;
}
