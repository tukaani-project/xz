// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       12_seekable_decompress.c
/// \brief      Decompress .xz file with limited random access
///
/// It's limited because acceptable performance requires that the offsets
/// are close enough to the beginnings of Blocks or that Blocks are small.
///
/// Usage:      ./12_seekable_decompress INFILE.xz [OFFSET LENGTH]...
///
/// Example:    ./12_seekable_decompress foo.xz 1000 50 200 10
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lzma.h>


#define INBUF_SIZE BUFSIZ
#define OUTBUF_SIZE BUFSIZ


// Get lzma_index for the input .xz file. This function is based
// on 11_file_info.c.
//
// On success, return a pointer to lzma_index. The caller must free it
// using lzma_index_end(idx, NULL) when the lzma_index is no longer needed.
static lzma_index *
read_file_info(FILE *infile, const char *filename)
{
	// Get the file size. In standard C it can be done by seeking to
	// the end of the file and then getting the file position.
	// In POSIX one can use fstat() and then st_size from struct stat.
	// Also note that fseek() and ftell() use long and thus don't support
	// large files on 32-bit systems (POSIX versions fseeko() and
	// ftello() can support large files).
	if (fseek(infile, 0, SEEK_END)) {
		fprintf(stderr, "Error seeking the file '%s': %s\n",
				filename, strerror(errno));
		return NULL;
	}

	const long file_size = ftell(infile);

	// The decoder wants to start from the beginning of the .xz file.
	rewind(infile);

	// Initialize the decoder.
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *idx;
	lzma_ret ret = lzma_file_info_decoder(&strm, &idx, UINT64_MAX,
			(uint64_t)file_size);
	switch (ret) {
	case LZMA_OK:
		// Initialization succeeded.
		break;

	case LZMA_MEM_ERROR:
		fprintf(stderr, "Out of memory when initializing "
				"the .xz file info decoder\n");
		return NULL;

	case LZMA_PROG_ERROR:
	default:
		fprintf(stderr, "Unknown error, possibly a bug\n");
		return NULL;
	}

	// This example program reuses the same lzma_stream structure,
	// so we need to reset this when starting a new file.
	strm.avail_in = 0;

	// Buffer for input data.
	uint8_t inbuf[INBUF_SIZE];

	// Pass data to the decoder and seek when needed.
	while (true) {
		if (strm.avail_in == 0) {
			strm.next_in = inbuf;
			strm.avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (ferror(infile)) {
				fprintf(stderr,
					"Error reading from '%s': %s\n",
					filename, strerror(errno));
				goto error;
			}

			// We don't need to care about hitting the end of
			// the file so no need to check for feof().
		}

		ret = lzma_code(&strm, LZMA_RUN);

		switch (ret) {
		case LZMA_OK:
			break;

		case LZMA_SEEK_NEEDED:
			// The cast is safe because liblzma won't ask us to
			// seek past the known size of the input file which
			// did fit into a long.
			//
			// NOTE: Remember to change these to off_t if you
			// switch fseeko() or lseek().
			if (fseek(infile, (long)(strm.seek_pos), SEEK_SET)) {
				fprintf(stderr, "Error seeking the "
						"file '%s': %s\n",
						filename, strerror(errno));
				goto error;
			}

			// The old data in the inbuf is useless now. Set
			// avail_in to zero so that we will read new input
			// from the new file position on the next iteration
			// of this loop.
			strm.avail_in = 0;
			break;

		case LZMA_STREAM_END:
			// File information was successfully decoded.
			lzma_end(&strm);
			return idx;

		case LZMA_FORMAT_ERROR:
			// .xz magic bytes weren't found.
			fprintf(stderr, "The file '%s' is not "
					"in the .xz format\n", filename);
			goto error;

		case LZMA_OPTIONS_ERROR:
			fprintf(stderr, "The file '%s' has .xz headers that "
					"are not supported by this liblzma "
					"version\n", filename);
			goto error;

		case LZMA_DATA_ERROR:
			fprintf(stderr, "The file '%s' is corrupt\n",
					filename);
			goto error;

		case LZMA_MEM_ERROR:
			fprintf(stderr, "Memory allocation failed when "
					"decoding the file '%s'\n", filename);
			goto error;

		// LZMA_MEMLIMIT_ERROR shouldn't happen because we used
		// UINT64_MAX as the limit.
		//
		// LZMA_BUF_ERROR shouldn't happen because we always provide
		// new input when the input buffer is empty. The decoder
		// knows the input file size and thus won't try to read past
		// the end of the file.
		case LZMA_MEMLIMIT_ERROR:
		case LZMA_BUF_ERROR:
		case LZMA_PROG_ERROR:
		default:
			fprintf(stderr, "Unknown error, possibly a bug\n");
			goto error;
		}
	}

	// This line is never reached.

error:
	lzma_end(&strm);
	return NULL;
}


static bool
decode_from_offset(lzma_stream *strm, uint8_t inbuf[static INBUF_SIZE],
		FILE *infile, const char *filename,
		uint64_t offset, uint64_t len)
{
	// Buffer for output data. The input buffer we get as an argument
	// to preserve read-but-unused input between calls to this function.
	uint8_t outbuf[OUTBUF_SIZE];

	// strm->next_out and strm->avail_out are set in the loop below.
	// It's simpler to not duplicate it here.
	strm->avail_out = 0;

	// strm->next_in and ->avail_out aren't touched here. On the first
	// call to this function, avail_in == 0, and on later calls there
	// may be unused bytes waiting to be decoded. The unused bytes are
	// discarded if we need to seek the input; otherwise the decoder
	// can use them to continue decoding where it stopped earlier.

	// Seek to the requested uncompressed offset. (To seek to a specific
	// Block by its number in the file, use LZMA_SEEK_TO_BLOCK instead
	// and set the Block number in strm->seek_pos.)
	lzma_action action = LZMA_SEEK_TO_OFFSET;
	strm->seek_pos = offset;

	while (len > 0) {
		// Read more input if all input has been consumed.
		// However, don't do this on the first iteration of
		// this loop (action != LZMA_RUN) because we might
		// need to seek the input.
		if (strm->avail_in == 0 && action == LZMA_RUN) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, INBUF_SIZE, infile);

			if (ferror(infile)) {
				fprintf(stderr,
					"Error reading from '%s': %s\n",
					filename, strerror(errno));
				return false;
			}
		}

		// Provide more output space. Note that on the first
		// iteration of the loop strm->avail_out equals 0.
		if (strm->avail_out == 0) {
			strm->next_out = outbuf;

			// Don't provide more space than needed to decode
			// the requested amount.
			strm->avail_out = len < OUTBUF_SIZE
					? len : OUTBUF_SIZE;
		}

		// On the first iteration of the loop, the 'action' argument
		// will tell lzma_code() to seek.
		const lzma_ret ret = lzma_code(strm, action);

		// The seek request has been passed to lzma_code().
		// If more data is needed, the later lzma_code() calls
		// will decode more data normally.
		action = LZMA_RUN;

		// Write the decoded data out if the output buffer became
		// full or if the end of the file was reached. Due to how
		// we have set strm->avail_out, LZMA_STREAM_END is checked
		// here only write the last bytes out in case len was so
		// large that we would need to read past the end of the file.
		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			// Use pointer arithmetic instead of calculating
			// OUTBUF_SIZE - avail_out because when
			// len < OUTBUF_SIZE, we set avail_out = len.
			size_t write_size = (size_t)(strm->next_out - outbuf);
			len -= write_size;

			if (fwrite(outbuf, 1, write_size, stdout)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
				strerror(errno));
				return false;
			}
		}

		switch (ret) {
		case LZMA_OK:
			break;

		case LZMA_SEEK_NEEDED:
			// Even if we don't use LZMA_SEEK_TO_OFFSET or
			// LZMA_SEEK_TO_BLOCK, it's possible that
			// LZMA_SEEK_NEEDED is still returned under
			// some conditions.
			//
			// The cast is safe because liblzma won't ask us to
			// seek past the known size of the input file which
			// did fit into a long.
			//
			// NOTE: Remember to change these to off_t if you
			// switch fseeko() or lseek().
			if (fseek(infile, (long)(strm->seek_pos), SEEK_SET)) {
				fprintf(stderr, "Error seeking the "
						"file '%s': %s\n",
						filename, strerror(errno));
				return NULL;
			}

			// The old data in the inbuf is useless now. Set
			// avail_in to zero so that we will read new input
			// from the new file position on the next iteration
			// of this loop.
			strm->avail_in = 0;
			break;

		case LZMA_SEEK_ERROR:
			// The requested position is greater than or equal
			// to the uncompressed size of the file.
			//
			// This error is recoverable: the input and output
			// positions of the lzma_stream weren't modified,
			// and one can either continue decoding from the
			// old position or try to seek again.
			fprintf(stderr, "The specified uncompressed offset "
					"%" PRIu64 " is at or past the end of "
					"the file '%s'\n", offset, filename);
			return false;

		case LZMA_STREAM_END:
			// End of file was reached. If we didn't produce as
			// much data as requested, consider it an error.
			if (len != 0) {
				fprintf(stderr,
					"Cannot read %" PRIu64 " byte(s) "
					"past the end of the file '%s'\n",
					len, filename);
				return false;
			}

			return true;

		case LZMA_OPTIONS_ERROR:
			fprintf(stderr, "The file '%s' has .xz headers that "
					"are not supported by this liblzma "
					"version\n", filename);
			return false;

		case LZMA_DATA_ERROR:
			fprintf(stderr, "The file '%s' is corrupt\n",
					filename);
			return false;

		case LZMA_MEM_ERROR:
			fprintf(stderr, "Memory allocation failed when "
					"decoding the file '%s'\n", filename);
			return false;

		// LZMA_MEMLIMIT_ERROR shouldn't happen because we used
		// UINT64_MAX as the limit.
		//
		// LZMA_BUF_ERROR shouldn't happen because we always provide
		// new input when the input buffer is empty. The decoder
		// knows the input file size and thus won't try to read past
		// the end of the file.
		case LZMA_MEMLIMIT_ERROR:
		case LZMA_BUF_ERROR:
		case LZMA_PROG_ERROR:
		default:
			fprintf(stderr, "Unknown error, possibly a bug\n");
			return false;
		}
	}

	// Decoding was successful.
	return true;
}


static bool
str_to_uint64(uint64_t *result, const char *str)
{
	*result = 0;

	char *endptr;
	errno = 0;
	*result = strtoull(str, &endptr, 10);

	if (*str == '\0' || *endptr != '\0') {
		fprintf(stderr, "Not a decimal integer: %s\n", str);
		return false;
	}

	if (errno == ERANGE) {
		fprintf(stderr, "Integer is too large: %s\n", str);
		return false;
	}

	return true;
}


extern int
main(int argc, char **argv)
{
	// We need one filename and an even number of integer arguments.
	if (argc < 2 || (argc % 2) != 0) {
		fprintf(stderr, "Usage: %s FILE.xz [OFFSET LENGTH]...\n",
				argv[0]);
		return EXIT_FAILURE;
	}

	FILE *infile = fopen(argv[1], "rb");
	if (infile == NULL) {
		fprintf(stderr, "Cannot open the file '%s': %s\n",
				argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	lzma_index *index = read_file_info(infile, argv[1]);
	if (index == NULL) {
		// Error message was already printed.
		fclose(infile);
		return EXIT_FAILURE;
	}

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_seekable_decoder(&strm, UINT64_MAX, 0, index);
	if (ret != LZMA_OK) {
		fprintf(stderr, "Error initializing seekable decoder\n");
		lzma_index_end(index, NULL);
		fclose(infile);
		return EXIT_FAILURE;
	}

	// decode_from_offset() reads from infile in INBUF_SIZE chunks.
	// Often the last chunk won't be consumed completely by the decoder.
	// On the next call to decode_from_offset() the remaining data may
	// be needed though. Thus, keep the same inbuf available between
	// the calls to decode_from_offset().
	uint8_t inbuf[INBUF_SIZE];

	// Initially there is no already-read input. The initialization with
	// LZMA_STREAM_INIT already set these and so in this example code
	// these are redundant. If one was reusing the same lzma_stream,
	// then strm.avail_in = 0 would be required here so that the newly-
	// initialized decoder won't see stale data.
	strm.next_in = NULL;
	strm.avail_in = 0;

	// If errors occur, this will be set to false.
	bool success = true;

	for (int i = 2; i < argc; i += 2) {
		uint64_t offset;
		uint64_t len;

		if (!str_to_uint64(&offset, argv[i])) {
			success = false;
			break;
		}

		if (!str_to_uint64(&len, argv[i + 1])) {
			success = false;
			break;
		}

		if (!decode_from_offset(
				&strm, inbuf, infile, argv[1], offset, len)) {
			success = false;
			break;
		}
	}

	// Free memory and close the input file.
	lzma_end(&strm);
	lzma_index_end(index, NULL);
	fclose(infile);

	// Close stdout to catch possible write errors that can occur
	// when pending data is flushed from the stdio buffers.
	if (success && fclose(stdout)) {
		fprintf(stderr, "Write error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
