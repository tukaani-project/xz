// SPDX-License-Identifier: 0BSD
/*
 * fuzz_decode_index.c
 *
 * Fuzz target for lzma_index decoder (liblzma).
 *
 * The existing OSS-Fuzz harnesses cover:
 *   fuzz_decode_stream    - full .xz stream decoder
 *   fuzz_decode_stream_mt - multithreaded .xz stream decoder
 *   fuzz_decode_alone     - legacy .lzma stream decoder
 *   fuzz_encode_stream    - .xz stream encoder
 *
 * The lzma_index decoder (src/liblzma/common/index_decoder.c) has NO
 * dedicated fuzz coverage. The index stores block sizes and stream padding
 * and is parsed during random-access seeks; a bug here could allow
 * out-of-bounds reads or integer overflows when seeking into crafted
 * archives without decompressing them first.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"
#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *inbuf, size_t insize)
{
	lzma_index *idx = NULL;
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret;

	/* Decode index from raw fuzz input */
	uint64_t memlimit = MEM_LIMIT;
	ret = lzma_index_decoder(&strm, &idx, memlimit);
	if (ret != LZMA_OK)
		goto cleanup;

	strm.next_in  = inbuf;
	strm.avail_in = insize;

	ret = lzma_code(&strm, LZMA_FINISH);

	/* LZMA_STREAM_END = success, anything else = malformed input */
	if (ret == LZMA_STREAM_END && idx != NULL) {
		/* Exercise the index iterator to reach block-offset code */
		lzma_index_iter iter;
		lzma_index_iter_init(&iter, idx);

		/* Walk streams */
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
			;

		/* Walk blocks */
		lzma_index_iter_rewind(&iter);
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK))
			;

		/* Probe random-access seek at byte 0 */
		lzma_index_iter_rewind(&iter);
		lzma_index_iter_locate(&iter, 0);
	}

cleanup:
	lzma_end(&strm);
	if (idx)
		lzma_index_end(idx, NULL);

	return 0;
}
