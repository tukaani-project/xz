///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32.c
/// \brief      Primitive CRC32 calculation tool
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

#include "sysdefs.h"
#include <stdio.h>


int
main(void)
{
	uint32_t crc = 0;

	do {
		uint8_t buf[BUFSIZ];
		const size_t size = fread(buf, 1, sizeof(buf), stdin);
		crc = lzma_crc32(buf, size, crc);
	} while (!ferror(stdin) && !feof(stdin));

	//printf("%08" PRIX32 "\n", crc);

	// I want it little endian so it's easy to work with hex editor.
	printf("%02" PRIX32 " ", crc & 0xFF);
	printf("%02" PRIX32 " ", (crc >> 8) & 0xFF);
	printf("%02" PRIX32 " ", (crc >> 16) & 0xFF);
	printf("%02" PRIX32 " ", crc >> 24);
	printf("\n");

	return 0;
}
