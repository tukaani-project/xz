// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_decode_lzip.c
/// \brief      Fuzz test program for liblzma .lz (lzip) decoding
//
//  Author:     Nikhil Bharti
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "lzma.h"
#include "fuzz_common.h"


extern int
LLVMFuzzerTestOneInput(const uint8_t *inbuf, size_t inbuf_size)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	prepare_stream(&strm, inbuf, inbuf_size);

	const uint32_t flags = LZMA_TELL_ANY_CHECK | LZMA_IGNORE_CHECK
			| LZMA_CONCATENATED;
	lzma_ret ret;

	for (int i = 0; i < 3; ++i) {
		ret = lzma_lzip_decoder(&strm, MEM_LIMIT, flags);

		if (ret == LZMA_MEM_ERROR)
			continue;

		if (ret != LZMA_OK) {
			fprintf(stderr, "lzma_lzip_decoder() failed (%d)\n",
					ret);
			abort();
		}

		fuzz_code(&strm, strm.next_in, strm.avail_in);
	}

	lzma_end(&strm);
	return 0;
}
