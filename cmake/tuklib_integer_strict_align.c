/* SPDX-License-Identifier: 0BSD */

#include <string.h>

int check_strict_align(const void *p)
{
	int i;
	memcpy(&i, p, sizeof(i));
	return i;
}
