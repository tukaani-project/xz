// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       07_custom_allocator.c
/// \brief      Demonstrate compression using a custom memory allocator
///
/// Usage:      ./07_custom_allocator < INPUT_FILE > OUT_FILE.xz
///
//  Author:     Ali Ammar
//
///////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lzma.h>

// A simple structure to track the total memory allocated and the number
// of active allocations.
typedef struct {
	size_t total_allocated;
	size_t peak_allocated;
	size_t active_allocs;
} allocator_stats;

// Custom memory allocation function.
//
// In liblzma, 'nmemb' is always 1, but we multiply it by 'size' for safety
// and compatibility.
static void *
custom_alloc(void *opaque, size_t nmemb, size_t size)
{
	allocator_stats *stats = opaque;
	size_t total_size = nmemb * size;

	// Allocate memory. We include extra bytes at the beginning to store
	// the size of the allocation so that custom_free knows how much memory
	// is being released.
	void *ptr = malloc(total_size + sizeof(size_t));
	if (ptr == NULL)
		return NULL;

	// Store the allocation size.
	*(size_t *)ptr = total_size;

	// Update stats.
	stats->total_allocated += total_size;
	stats->active_allocs++;
	if (stats->total_allocated > stats->peak_allocated)
		stats->peak_allocated = stats->total_allocated;

	// Return the pointer past the stored size.
	return (void *)((char *)ptr + sizeof(size_t));
}

// Custom memory freeing function.
static void
custom_free(void *opaque, void *ptr)
{
	if (ptr == NULL)
		return;

	allocator_stats *stats = opaque;

	// Retrieve the original pointer and stored size.
	void *orig_ptr = (void *)((char *)ptr - sizeof(size_t));
	size_t allocation_size = *(size_t *)orig_ptr;

	// Update stats.
	stats->total_allocated -= allocation_size;
	stats->active_allocs--;

	// Free the memory.
	free(orig_ptr);
}

static bool
compress(lzma_stream *strm, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (ferror(infile)) {
				fprintf(stderr, "Read error: %s\n",
						strerror(errno));
				return false;
			}

			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_DATA_ERROR:
				msg = "Size limits of the format were exceeded";
				break;

			default:
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "Encoder error: %s (error code %u)\n",
					msg, ret);
			return false;
		}
	}
}

int
main(void)
{
	allocator_stats stats = { 0, 0, 0 };

	// Initialize the custom allocator structure.
	lzma_allocator allocator = {
		.alloc = custom_alloc,
		.free = custom_free,
		.opaque = &stats
	};

	// Initialize the stream.
	lzma_stream strm = LZMA_STREAM_INIT;

	// Assign our custom allocator before calling any liblzma functions.
	strm.allocator = &allocator;

	// Initialize the encoder. It will use our custom allocator.
	// We use a lower preset (preset 2) to reduce memory usage for this example.
	lzma_ret ret = lzma_easy_encoder(&strm, 2, LZMA_CHECK_CRC64);
	if (ret != LZMA_OK) {
		fprintf(stderr, "Error initializing the encoder: %u\n", ret);
		return EXIT_FAILURE;
	}

	// Compress stdin to stdout.
	bool success = compress(&strm, stdin, stdout);

	// Free the encoder's internal state. This will call custom_free() for
	// all allocated memory.
	lzma_end(&strm);

	// Print memory statistics to stderr.
	fprintf(stderr, "--- Custom Allocator Stats ---\n");
	fprintf(stderr, "Peak Memory Used:   %zu bytes\n", stats.peak_allocated);
	fprintf(stderr, "Active Allocations: %zu\n", stats.active_allocs);
	fprintf(stderr, "Total Leaked:       %zu bytes\n", stats.total_allocated);

	if (stats.total_allocated > 0) {
		fprintf(stderr, "Warning: Memory leaks detected!\n");
		success = false;
	}

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
