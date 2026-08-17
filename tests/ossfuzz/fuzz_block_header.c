// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_block_header.c
/// \brief      Fuzz test for Block Header decoding
///
/// Fuzzes lzma_block_header_decode(). Block headers carry the filter chain
/// used to compress each Block, making their parsing security-critical: a
/// crafted header could trigger memory-safety bugs when constructing filter
/// option structures or walking variable-length filter lists.
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lzma.h"


extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// A Block Header is at least 8 bytes (minimum size encoded in the
	// first byte via lzma_block_header_size_decode).  We need at least
	// one byte to read the header-size field.
	if (size < 8)
		return 0;

	// The first byte of a Block Header encodes the header size:
	//   header_size = (data[0] + 1) * 4
	// Valid range is 8..1024.  If data[0] is 0xFF the decoded size would
	// be 1024; we cap the required input at size to avoid reading past
	// the fuzzer buffer.
	uint32_t header_size = lzma_block_header_size_decode(data[0]);

	if (header_size > size)
		return 0;

	// We need a writable copy because lzma_block_header_decode() reads
	// from the buffer and lzma_block may need to be zeroed.
	uint8_t *buf = (uint8_t *)malloc(header_size);
	if (buf == NULL)
		abort();
	memcpy(buf, data, header_size);

	// Allocate the filter array that lzma_block_header_decode() will
	// populate.  It must have LZMA_FILTERS_MAX + 1 elements.
	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	// Initialise to LZMA_VLI_UNKNOWN so the decoder knows which slots
	// are unused (required by the API contract).
	for (int i = 0; i <= LZMA_FILTERS_MAX; i++) {
		filters[i].id = LZMA_VLI_UNKNOWN;
		filters[i].options = NULL;
	}

	lzma_block block;
	memset(&block, 0, sizeof(block));
	block.version = 0;
	block.filters = filters;
	// Tell the decoder how large the header is.
	block.header_size = header_size;

	lzma_ret ret = lzma_block_header_decode(&block, NULL, buf);

	if (ret == LZMA_OK) {
		// If decode succeeded, free any filter options the decoder
		// allocated.  lzma_filters_free() handles NULL options
		// pointers safely.
		lzma_filters_free(filters, NULL);
	}

	free(buf);

	return 0;
}
