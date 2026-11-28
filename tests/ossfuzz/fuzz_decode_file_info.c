// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_decode_file_info.c
/// \brief      Fuzz test program for liblzma .xz file info decoding
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
	(void)fuzz_code;
	lzma_stream strm = LZMA_STREAM_INIT;
	prepare_stream(&strm, inbuf, inbuf_size);

	for (int i = 0; i < 2; ++i) {
		lzma_index *idx = (void *)1;
		lzma_ret ret = lzma_file_info_decoder(
				&strm, &idx, MEM_LIMIT, inbuf_size);

		if (ret == LZMA_MEM_ERROR)
			continue;

		if (ret != LZMA_OK) {
			fprintf(stderr,
					"lzma_file_info_decoder() failed (%d)\n",
					ret);
			abort();
		}

		size_t pos = 0;
		unsigned int steps = 0;
		strm.next_in = NULL;
		strm.avail_in = 0;
		strm.next_out = NULL;
		strm.avail_out = 0;

		while (steps++ < 1024) {
			if (strm.avail_in == 0 && pos < inbuf_size) {
				size_t rem = inbuf_size - pos;
				size_t chunk = rem < IN_CHUNK_SIZE
						? rem : IN_CHUNK_SIZE;
				strm.next_in = inbuf + pos;
				strm.avail_in = chunk;
				pos += chunk;
			}

			ret = lzma_code(&strm, LZMA_RUN);
			if (ret == LZMA_SEEK_NEEDED) {
				if (strm.seek_pos > inbuf_size) {
					fprintf(stderr,
							"Invalid seek_pos\n");
					abort();
				}
				pos = (size_t)strm.seek_pos;
				strm.next_in = NULL;
				strm.avail_in = 0;
				continue;
			}

			if (ret == LZMA_PROG_ERROR) {
				fprintf(stderr,
						"lzma_code() returned "
						"LZMA_PROG_ERROR\n");
				abort();
			}

			if (ret != LZMA_OK)
				break;
		}

		if (ret == LZMA_STREAM_END)
			lzma_index_end(idx, NULL);
	}

	lzma_end(&strm);
	return 0;
}
