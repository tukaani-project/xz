// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_str_to_filters.c
/// \brief      Fuzz test program for lzma_str_to_filters()
//
//  Author:     Nikhil Bharti
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lzma.h"


extern int
LLVMFuzzerTestOneInput(const uint8_t *inbuf, size_t inbuf_size)
{
	if (inbuf_size < 2 || inbuf_size > 4096)
		return 0;

	const uint32_t to_flags = inbuf[0] & (LZMA_STR_ALL_FILTERS
			| LZMA_STR_NO_VALIDATION);
	const uint32_t from_flags = inbuf[1] & (LZMA_STR_ENCODER
			| LZMA_STR_DECODER | LZMA_STR_GETOPT_LONG
			| LZMA_STR_NO_SPACES);

	char str[4096];
	size_t len = inbuf_size - 2;
	memcpy(str, inbuf + 2, len);
	str[len] = '\0';

	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	int error_pos = 0;

	const char *err = lzma_str_to_filters(
			str, &error_pos, filters, to_flags, NULL);
	if (err == NULL) {
		char *out = NULL;
		lzma_ret ret = lzma_str_from_filters(
				&out, filters, from_flags, NULL);
		if (ret == LZMA_OK)
			free(out);
		lzma_filters_free(filters, NULL);
	}

	return 0;
}
