///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_encode_stream.c
/// \brief      Fuzz test program for liblzma lzma_stream_encoder() w/ LZMA2
//
//  Author:     Maksym Vatsyk
//
//  Based on Lasse Collin's original fuzzer for liblzma
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
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
	if (inbuf_size == 0) {
		fprintf(stderr, "no input data provided\n");
		return 0;
	}

	// set LZMA preset level based on the first input byte
	uint32_t preset_level;
	uint8_t decider = inbuf[0];
	switch (decider) {
	case 0:
	case 1:
	case 5:
		preset_level = (uint32_t)decider;
		break;
	case 6:
		preset_level = 0 | LZMA_PRESET_EXTREME;
		break;
	case 7:
		preset_level = 3 | LZMA_PRESET_EXTREME;
		break;
	default:
		return 0;
	}

	// Initialize lzma_options with the above preset level
	lzma_options_lzma opt_lzma;
	if (lzma_lzma_preset(&opt_lzma, preset_level)){
		fprintf(stderr, "lzma_lzma_preset() failed\n");
		abort();
	}

	// Initialize filter chain for lzma_stream_decoder() call
	// Use single LZMA2 filter for encoding
	lzma_filter filters[2];
	filters[0].id = LZMA_FILTER_LZMA2;
	filters[0].options = &opt_lzma;
	filters[1].id = LZMA_VLI_UNKNOWN;

	// initialize empty LZMA stream
	lzma_stream strm = LZMA_STREAM_INIT;

	// Initialize the stream encoder using the above
	// stream, filter chain and CRC64.
	if (lzma_stream_encoder(&strm,
			filters, LZMA_CHECK_CRC64) != LZMA_OK) {
		fprintf(stderr, "lzma_stream_encoder() failed\n");
		abort();
	}

	fuzz_code(&strm, inbuf  + 1, inbuf_size - 1);

	// Free the allocated memory.
	lzma_end(&strm);
	return 0;
}
