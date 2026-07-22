// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_index_decoder.c
/// \brief      Fuzz test for the .xz Index decoder
///
/// Fuzzes the Index decoder API using both the single-call
/// lzma_index_buffer_decode() and the streaming lzma_index_decoder()
/// interfaces. The Index is a critical structure at the end of .xz streams
/// that describes Block boundaries; parsing bugs here can affect the entire
/// random-access and integrity-checking path.
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lzma.h"

// Memory limit for index decoding (300 MiB)
#define MEM_LIMIT (300 << 20)


extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 4)
		return 0;

	// ----------------------------------------------------------------
	// Path 1: single-call lzma_index_buffer_decode()
	// ----------------------------------------------------------------
	{
		lzma_index *idx = NULL;
		uint64_t memlimit = MEM_LIMIT;
		size_t in_pos = 0;

		lzma_ret ret = lzma_index_buffer_decode(
				&idx, &memlimit, NULL,
				data, &in_pos, size);

		if (ret == LZMA_OK && idx != NULL) {
			// Walk the decoded index to exercise the iterator
			// code paths and ensure the structure is consistent.
			lzma_index_iter iter;
			lzma_index_iter_init(&iter, idx);

			while (!lzma_index_iter_next(
					&iter, LZMA_INDEX_ITER_ANY)) {
				// Just iterate; no result checking needed.
				(void)iter.stream.number;
				(void)iter.block.number_in_file;
			}

			lzma_index_end(idx, NULL);
		}
	}

	// ----------------------------------------------------------------
	// Path 2: streaming lzma_index_decoder()
	// ----------------------------------------------------------------
	{
		lzma_index *idx = NULL;
		lzma_stream strm = LZMA_STREAM_INIT;

		lzma_ret ret = lzma_index_decoder(&strm, &idx, MEM_LIMIT);
		if (ret != LZMA_OK) {
			// Allocation failure — abort as documented.
			fprintf(stderr,
				"lzma_index_decoder() failed (%d)\n", ret);
			abort();
		}

		strm.next_in = data;
		strm.avail_in = size;

		// Feed all input in one shot with LZMA_FINISH so the
		// decoder can signal LZMA_STREAM_END when done.
		ret = lzma_code(&strm, LZMA_FINISH);

		lzma_end(&strm);

		if (ret == LZMA_STREAM_END && idx != NULL)
			lzma_index_end(idx, NULL);
	}

	return 0;
}
