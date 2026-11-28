// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_decode_microlzma.c
/// \brief      Fuzz test program for liblzma MicroLZMA decoding
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
	if (inbuf_size < 4)
		return 0;

	const uint32_t dict_shift = 12 + (inbuf[0] & 0x07);
	const uint32_t dict_size = UINT32_C(1) << dict_shift;
	const uint64_t uncomp_size = (uint64_t)inbuf[1]
			| ((uint64_t)inbuf[2] << 8);
	const lzma_bool exact = (inbuf[3] & 1) != 0;

	const uint8_t *payload = inbuf + 4;
	const size_t payload_size = inbuf_size - 4;

	lzma_stream strm = LZMA_STREAM_INIT;
	prepare_stream(&strm, payload, payload_size);

	lzma_ret ret = lzma_microlzma_decoder(&strm, payload_size,
			uncomp_size, exact, dict_size);
	if (ret == LZMA_OK)
		fuzz_code(&strm, payload, payload_size);
	else if (ret == LZMA_PROG_ERROR) {
		fprintf(stderr,
				"lzma_microlzma_decoder() returned "
				"LZMA_PROG_ERROR\n");
		abort();
	}

	lzma_end(&strm);
	return 0;
}
