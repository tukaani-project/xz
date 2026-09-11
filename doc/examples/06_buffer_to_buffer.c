// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       06_buffer_to_buffer.c
/// \brief      Compress and decompress data in memory using buffers
///
/// Usage:      ./06_buffer_to_buffer
///
//  Author:     Ali Ammar
//
///////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lzma.h>

static bool
compress_buffer(const uint8_t *inbuf, size_t insize,
		uint8_t **outbuf, size_t *outsize)
{
	// Calculate the maximum possible size for the compressed data.
	// This ensures that the output buffer is large enough even if
	// the data is incompressible.
	size_t outbound = lzma_stream_buffer_bound(insize);
	if (outbound == 0) {
		fprintf(stderr, "Calculation of compressed buffer bound failed\n");
		return false;
	}

	*outbuf = malloc(outbound);
	if (*outbuf == NULL) {
		fprintf(stderr, "Memory allocation for compressed buffer failed\n");
		return false;
	}

	// Compress the data.
	size_t outpos = 0;
	lzma_ret ret = lzma_easy_buffer_encode(
			LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64, NULL,
			inbuf, insize, *outbuf, &outpos, outbound);

	if (ret != LZMA_OK) {
		const char *msg;
		switch (ret) {
		case LZMA_MEM_ERROR:
			msg = "Memory allocation failed";
			break;

		case LZMA_OPTIONS_ERROR:
			msg = "Specified preset is not supported";
			break;

		case LZMA_UNSUPPORTED_CHECK:
			msg = "Specified integrity check is not supported";
			break;

		default:
			msg = "Unknown error, possibly a bug";
			break;
		}

		fprintf(stderr, "Compression failed: %s (error code %u)\n",
				msg, ret);
		free(*outbuf);
		*outbuf = NULL;
		return false;
	}

	*outsize = outpos;
	return true;
}

static bool
decompress_buffer(const uint8_t *inbuf, size_t insize,
		uint8_t **outbuf, size_t *outsize, size_t original_size)
{
	// Allocate a buffer big enough to hold the uncompressed data.
	// In real-world applications, you might not know the exact
	// uncompressed size beforehand unless you stored it alongside
	// the compressed data, or you allocate a dynamic buffer that
	// grows as needed (which would require the multi-call API).
	*outbuf = malloc(original_size + 1); // +1 for null terminator if it's a string
	if (*outbuf == NULL) {
		fprintf(stderr, "Memory allocation for decompressed buffer failed\n");
		return false;
	}

	uint64_t memlimit = UINT64_MAX;
	size_t inpos = 0;
	size_t outpos = 0;

	// Decompress the data.
	lzma_ret ret = lzma_stream_buffer_decode(
			&memlimit, 0, NULL,
			inbuf, &inpos, insize,
			*outbuf, &outpos, original_size);

	if (ret != LZMA_OK) {
		const char *msg;
		switch (ret) {
		case LZMA_FORMAT_ERROR:
			msg = "Input is not in the .xz format";
			break;

		case LZMA_OPTIONS_ERROR:
			msg = "Unsupported compression options";
			break;

		case LZMA_DATA_ERROR:
			msg = "Compressed data is corrupt";
			break;

		case LZMA_BUF_ERROR:
			msg = "Output buffer is too small";
			break;

		case LZMA_MEM_ERROR:
			msg = "Memory allocation failed";
			break;

		case LZMA_MEMLIMIT_ERROR:
			msg = "Memory limit reached";
			break;

		default:
			msg = "Unknown error, possibly a bug";
			break;
		}

		fprintf(stderr, "Decompression failed: %s (error code %u)\n",
				msg, ret);
		free(*outbuf);
		*outbuf = NULL;
		return false;
	}

	*outsize = outpos;
	(*outbuf)[outpos] = '\0'; // Null terminate the output
	return true;
}

int
main(void)
{
	const char *message = "Hello! This is a buffer-to-buffer (in-memory) "
			"compression and decompression example using liblzma "
			"from the xz project. It is useful for cases where "
			"data is already in RAM.";
	size_t message_len = strlen(message);

	printf("Original message size: %zu bytes\n", message_len);

	uint8_t *compressed_buf = NULL;
	size_t compressed_size = 0;

	if (!compress_buffer((const uint8_t *)message, message_len,
			&compressed_buf, &compressed_size)) {
		return EXIT_FAILURE;
	}

	printf("Compressed size:      %zu bytes\n", compressed_size);

	uint8_t *decompressed_buf = NULL;
	size_t decompressed_size = 0;

	if (!decompress_buffer(compressed_buf, compressed_size,
			&decompressed_buf, &decompressed_size, message_len)) {
		free(compressed_buf);
		return EXIT_FAILURE;
	}

	printf("Decompressed size:    %zu bytes\n", decompressed_size);
	printf("Decompressed message: %s\n", (char *)decompressed_buf);

	// Verify that the decompressed message matches the original.
	if (decompressed_size != message_len ||
			strcmp(message, (char *)decompressed_buf) != 0) {
		fprintf(stderr, "Error: Decompressed message does not match original!\n");
		free(compressed_buf);
		free(decompressed_buf);
		return EXIT_FAILURE;
	}

	printf("Success!\n");

	free(compressed_buf);
	free(decompressed_buf);
	return EXIT_SUCCESS;
}
