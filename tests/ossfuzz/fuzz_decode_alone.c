///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_decode_auto.c
/// \brief      Fuzz test program for liblzma lzma_auto_decoder()
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
	lzma_stream strm = LZMA_STREAM_INIT;
	// Initialize a LZMA alone decoder using the memory usage limit
	// defined in fuzz_common.h
	if (lzma_alone_decoder(&strm, MEM_LIMIT) != LZMA_OK) {
		// This should never happen unless the system has
		// no free memory or address space to allow the small
		// allocations that the initialization requires.
		fprintf(stderr, "lzma_alone_decoder() failed\n");
		abort();
	}

	fuzz_code(&strm, inbuf, inbuf_size);

	// Free the allocated memory.
	lzma_end(&strm);
	return 0;
}
