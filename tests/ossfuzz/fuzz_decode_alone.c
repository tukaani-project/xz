///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_decode_alone.c
/// \brief      Fuzz test program for liblzma .lzma decoding
//
//  Authors:    Maksym Vatsyk
//              Lasse Collin
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
	lzma_ret ret = lzma_alone_decoder(&strm, MEM_LIMIT);

	if (ret != LZMA_OK) {
		// This should never happen unless the system has
		// no free memory or address space to allow the small
		// allocations that the initialization requires.
		fprintf(stderr, "lzma_alone_decoder() failed (%d)\n", ret);
		abort();
	}

	fuzz_code(&strm, inbuf, inbuf_size);

	// Free the allocated memory.
	lzma_end(&strm);
	return 0;
}
