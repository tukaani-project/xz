///////////////////////////////////////////////////////////////////////////////
//
/// \file       repeat.c
/// \brief      Repeats given string given times
///
/// This program can be useful when debugging run-length encoder in
/// the Subblock filter, especially the condition when repeat count
/// doesn't fit into 28-bit integer.
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
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s COUNT STRING\n", argv[0]);
		exit(1);
	}

	unsigned long long count = strtoull(argv[1], NULL, 10);
	const size_t size = strlen(argv[2]);

	while (count-- != 0)
		fwrite(argv[2], 1, size, stdout);

	return !!(ferror(stdout) || fclose(stdout));
}
