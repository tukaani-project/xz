// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       decode_byte_by_byte.c
/// \brief      Decodes a file one byte at a time
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"
#include "lzma.h"
#include <stdio.h>


#define MEMLIMIT (UINT64_C(5) << 20)

#define FAIL(msg) \
	do { \
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], msg); \
		return EXIT_FAILURE; \
	} while (0)


int
main(int argc, char **argv)
{
	if (argc != 2)
		FAIL("One filename is required");

	FILE *f = fopen(argv[1], "rb");
	if (f == NULL)
		FAIL("Opening the file failed");

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_auto_decoder(&strm, MEMLIMIT, LZMA_CONCATENATED) != LZMA_OK)
		FAIL("Decoder initialization failed");

	bool eof = false;

	while (true) {
		uint8_t in[1];
		uint8_t out[1];

		if (!eof) {
			const int b = fgetc(f);
			if (b == EOF) {
				if (ferror(f))
					FAIL("Error reading the input file");

				eof = true;
			} else {
				in[0] = (uint8_t)b;
				strm.next_in = in;
				strm.avail_in = 1;
			}
		}

		do {
			strm.next_out = out;
			strm.avail_out = 1;

			const lzma_action action
					= eof ? LZMA_FINISH : LZMA_RUN;
			const lzma_ret ret = lzma_code(&strm, action);

			if (strm.avail_out == 0) {
				if (fputc(out[0], stdout) == EOF)
					FAIL("Error writing to stdout");
			}

			if (ret == LZMA_STREAM_END)
				goto end;

			if (ret != LZMA_OK) {
				fprintf(stderr, "%s: %s: "
						"Decoder error (ret = %d)\n",
						argv[0], argv[1], ret);
				return EXIT_FAILURE;
			}
		} while (strm.avail_out == 0);
	}

end:
	// We must have consumed the entire file.
	if (fgetc(f) != EOF)
		FAIL("Garbage at the end of the file");

	if (ferror(f))
		FAIL("Error reading the input file (end)");

	lzma_end(&strm);
	(void)fclose(f);
	return EXIT_SUCCESS;
}
