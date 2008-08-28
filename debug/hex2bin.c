///////////////////////////////////////////////////////////////////////////////
//
/// \file       hex2bin.c
/// \brief      Converts hexadecimal input strings to binary
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"
#include <stdio.h>
#include <ctype.h>


static int
getbin(int x)
{
	if (x >= '0' && x <= '9')
		return x - '0';

	if (x >= 'A' && x <= 'F')
		return x - 'A' + 10;

	return x - 'a' + 10;
}


int
main(void)
{
	while (true) {
		int byte = getchar();
		if (byte == EOF)
			return 0;
		if (!isxdigit(byte))
			continue;

		const int digit = getchar();
		if (digit == EOF || !isxdigit(digit)) {
			fprintf(stderr, "Invalid input\n");
			return 1;
		}

		byte = (getbin(byte) << 4) | getbin(digit);
		if (putchar(byte) == EOF) {
			perror(NULL);
			return 1;
		}
	}
}
