///////////////////////////////////////////////////////////////////////////////
//
/// \file       fastpos_tablegen.c
/// \brief      Generates the lzma_fastpos[] lookup table
//
//  Copyright (C) 1999-2007 Igor Pavlov
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

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include "fastpos.h"


int
main(void)
{
	uint8_t fastpos[1 << FASTPOS_BITS];

	const uint8_t fast_slots = 2 * FASTPOS_BITS;
	uint32_t c = 2;

	fastpos[0] = 0;
	fastpos[1] = 1;

	for (uint8_t slot_fast = 2; slot_fast < fast_slots; ++slot_fast) {
		const uint32_t k = 1 << ((slot_fast >> 1) - 1);
		for (uint32_t j = 0; j < k; ++j, ++c)
			fastpos[c] = slot_fast;
	}

	printf("/* This file has been automatically generated "
			"by fastpos_tablegen.c. */\n\n"
			"#include \"common.h\"\n"
			"#include \"fastpos.h\"\n\n"
			"const uint8_t lzma_fastpos[1 << FASTPOS_BITS] = {");

	for (size_t i = 0; i < (1 << FASTPOS_BITS); ++i) {
		if (i % 16 == 0)
			printf("\n\t");

		printf("%3u", (unsigned int)(fastpos[i]));

		if (i != (1 << FASTPOS_BITS) - 1)
			printf(",");
	}

	printf("\n};\n");

	return 0;
}
