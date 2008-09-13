///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.c
/// \brief      Listing information about .lzma files
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"


/*

1. Check the file type: native, alone, unknown

Alone:
1. Show info about header. Don't look for concatenated parts.

Native:
1. Check that Stream Header is valid.
2. Seek to the end of the file.
3. Skip padding.
4. Reverse decode Stream Footer.
5. Seek Backward Size bytes.
6.

*/


static void
unsupported_file(file_handle *handle)
{
	errmsg(V_ERROR, "%s: Unsupported file type", handle->name);
	set_exit_status(ERROR);
	(void)io_close(handle);
	return;
}


/// Primitive escaping function, that escapes only ASCII control characters.
static void
print_escaped(const uint8_t *str)
{
	while (*str != '\0') {
		if (*str <= 0x1F || *str == 0x7F)
			printf("\\x%02X", *str);
		else
			putchar(*str);

		++str;
	}

	return;
}


static void
list_native(file_handle *handle)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_stream_flags flags;
	lzma_ret ret = lzma_stream_header_decoder(&strm, &flags);

}


static void
list_alone(const listing_handle *handle)
{
	if (handle->buffer[0] > (4 * 5 + 4) * 9 + 8) {
		unsupported_file(handle);
		return;
	}

	const unsigned int pb = handle->buffer[0] / (9 * 5);
	handle->buffer[0] -= pb * 9 * 5;
	const unsigned int lp = handle->buffer[0] / 9;
	const unsigned int lc = handle->buffer[0] - lp * 9;

	uint32_t dict = 0;
	for (size_t i = 1; i < 5; ++i) {
		dict <<= 8;
		dict |= header[i];
	}

	if (dict > LZMA_DICTIONARY_SIZE_MAX) {
		unsupported_file(handle);
		return;
	}

	uint64_t uncompressed_size = 0;
	for (size_t i = 5; i < 13; ++i) {
		uncompressed_size <<= 8;
		uncompressed_size |= header[i];
	}

	// Reject files with uncompressed size of 256 GiB or more. It's
	// an arbitrary limit trying to avoid at least some false positives.
	if (uncompressed_size != UINT64_MAX
			&& uncompressed_size >= (UINT64_C(1) << 38)) {
		unsupported_file(handle);
		return;
	}

	if (verbosity < V_WARNING) {
		printf("name=");
		print_escaped(handle->name);
		printf("\nformat=alone\n");

		if (uncompressed_size == UINT64_MAX)
			printf("uncompressed_size=unknown\n");
		else
			printf("uncompressed_size=%" PRIu64 "\n",
					uncompressed_size);

		printf("dict=%" PRIu32 "\n", dict);

		printf("lc=%u\nlp=%u\npb=%u\n\n", lc, lp, pb);

	} else {
		printf("File name:                   ");
		print_escaped(handle->name);
		printf("\nFile format:                 LZMA_Alone\n")

		printf("Uncompressed size:           ");
		if (uncompressed_size == UINT64_MAX)
			printf("unknown\n");
		else
			printf("%," PRIu64 " bytes (%" PRIu64 " MiB)\n",
					uncompressed_size,
					(uncompressed_size + 1024 * 512)
						/ (1024 * 1024));

		printf("Dictionary size:             %," PRIu32 " bytes "
				"(%" PRIu32 " MiB)\n",
				dict, (dict + 1024 * 512) / (1024 * 1024));

		printf("Literal context bits (lc):   %u\n", lc);
		printf("Literal position bits (lc):  %u\n", lp);
		printf("Position bits (pb):          %u\n", pb);
	}

	return;
}




typedef struct {
	const char *filename;
	struct stat st;
	int fd;

	lzma_stream strm;
	lzma_stream_flags stream_flags;
	lzma_info *info;

	lzma_vli backward_size;
	lzma_vli uncompressed_size;

	size_t buffer_size;
	uint8_t buffer[IO_BUFFER_SIZE];
} listing_handle;


static bool
listing_pread(listing_handle *handle, uint64_t offset)
{
	if (offset >= (uint64_t)(handle->st.st_size)) {
		errmsg(V_ERROR, "%s: Trying to read past the end of "
				"the file.", handle->filename);
		return true;
	}

#ifdef HAVE_PREAD
	const ssize_t ret = pread(handle->fd, handle->buffer, IO_BUFFER_SIZE,
			(off_t)(offset));
#else
	// Use lseek() + read() since we don't have pread(). We don't care
	// to which offset the reading position is left.
	if (lseek(handle->fd, (off_t)(offset), SEEK_SET) == -1) {
		errmsg(V_ERROR, "%s: %s", handle->filename, strerror(errno));
		return true;
	}

	const ssize_t ret = read(handle->fd, handle->buffer, IO_BUFFER_SIZE);
#endif

	if (ret == -1) {
		errmsg(V_ERROR, "%s: %s", handle->filename, strerror(errno));
		return true;
	}

	if (ret == 0) {
		errmsg(V_ERROR, "%s: Trying to read past the end of "
				"the file.", handle->filename);
		return true;
	}

	handle->buffer_size = (size_t)(ret);
	return false;
}



static bool
parse_stream_header(listing_handle *handle)
{
	if (listing_pread(handle, 0))
		return true;

	// TODO Got enough input?

	lzma_ret ret = lzma_stream_header_decoder(
			&handle->strm, &handle->stream_flags);
	if (ret != LZMA_OK) {
		errmsg(V_ERROR, "%s: %s", handle->name, str_strm_error(ret));
		return true;
	}

	handle->strm.next_in = handle->buffer;
	handle->strm.avail_in = handle->buffer_size;
	ret = lzma_code(&handle->strm, LZMA_RUN);
	if (ret != LZMA_STREAM_END) {
		assert(ret != LZMA_OK);
		errmsg(V_ERROR, "%s: %s", handle->name, str_strm_error(ret));
		return true;
	}

	return false;
}


static bool
parse_stream_tail(listing_handle *handle)
{
	uint64_t offset = (uint64_t)(handle->st.st_size);

	// Skip padding
	do {
		if (offset == 0) {
			errmsg(V_ERROR, "%s: %s", handle->name,
					str_strm_error(LZMA_DATA_ERROR));
			return true;
		}

		if (offset < IO_BUFFER_SIZE)
			offset = 0;
		else
			offset -= IO_BUFFER_SIZE;

		if (listing_pread(handle, offset))
			return true;

		while (handle->buffer_size > 0
				&& handle->buffer[handle->buffer_size - 1]
					== '\0')
			--handle->buffer_size;

	} while (handle->buffer_size == 0);

	if (handle->buffer_size < LZMA_STREAM_TAIL_SIZE) {
		// TODO
	}

	lzma_stream_flags stream_flags;
	lzma_ret ret = lzma_stream_tail_decoder(&handle->strm, &stream_flags);
	if (ret != LZMA_OK) {
		errmsg(V_ERROR, "%s: %s", handle->name, str_strm_error(ret));
		return true;
	}

	handle->strm.next_in = handle->buffer + handle->buffer_size
			- LZMA_STREAM_TAIL_SIZE;
	handle->strm.avail_in = LZMA_STREAM_TAIL_SIZE;
	handle->buffer_size -= LZMA_STREAM_TAIL_SIZE;
	ret = lzma_code(&handle->strm, LZMA_RUN);
	if (ret != LZMA_OK) {
		assert(ret != LZMA_OK);
		errmsg(V_ERROR, "%s: %s", handle->name, str_strm_error(ret));
		return true;
	}

	if (!lzma_stream_flags_is_equal(handle->stream_flags, stream_flags)) {
		// TODO
		// Possibly corrupt, possibly concatenated file.
	}

	handle->backward_size = 0;
	ret = lzma_vli_reverse_decode(&handle->backward_size, handle->buffer,
			&handle->buffer_size);
	if (ret != LZMA_OK) {
		// It may be LZMA_BUF_ERROR too, but it doesn't make sense
		// as an error message displayed to the user.
		errmsg(V_ERROR, "%s: %s", handle->name,
				str_strm_error(LZMA_DATA_ERROR));
		return true;
	}

	if (!stream_flags.is_multi) {
		handle->uncompressed_size = 0;
		size_t tmp = handle->buffer_size;
		ret = lzma_vli_reverse_decode(&handle->uncompressed_size,
				handle->buffer, &tmp);
		if (ret != LZMA_OK)
			handle->uncompressed_size = LZMA_VLI_UNKNOWN;
	}

	// Calculate the Header Metadata Block start offset.


	return false;
}



static void
list_native(listing_handle *handle)
{
	lzma_memory_limiter *limiter
			= lzma_memory_limiter_create(opt_memory);
	if (limiter == NULL) {
		errmsg(V_ERROR,
	}
	lzma_info *info =


	// Parse Stream Header
	//
	// Single-Block Stream:
	//  - Parse Block Header
	//  - Parse Stream Footer
	//  - If Backward Size doesn't match, error out
	//
	// Multi-Block Stream:
	//  - Parse Header Metadata Block, if any
	//  - Parse Footer Metadata Block
	//  - Parse Stream Footer
	//  - If Footer Metadata Block doesn't match the Stream, error out
	//
	// In other words, we don't support concatened files.
	if (parse_stream_header(handle))
		return;

	if (parse_block_header(handle))
		return;

	if (handle->stream_flags.is_multi) {
		if (handle->block_options.is_metadata) {
			if (parse_metadata(handle)
				return;
		}

		if (my_seek(handle,

	} else {
		if (handle->block_options.is_metadata) {
			FILE_IS_CORRUPT();
			return;
		}

		if (parse_stream_footer(handle))
			return;

		// If Uncompressed Size isn't present in Block Header,
		// it must be present in Stream Footer.
		if (handle->block_options.uncompressed_size
					== LZMA_VLI_UNKNOWN
				&& handle->stream_flags.uncompressed_size
					== LZMA_VLI_UNKNOWN) {
			FILE_IS_CORRUPT();
			return;
		}

		// Construct a single-Record Index.
		lzma_index *index = malloc(sizeof(lzma_index));
		if (index == NULL) {
			out_of_memory();
			return;
		}

		// Pohdintaa:
		// Jos Block coder hoitaisi Uncompressed ja Backward Sizet,
		// voisi index->total_sizeksi laittaa suoraan Backward Sizen.
		index->total_size =

		if () {

		}
	}


	if (handle->block_options.is_metadata) {
		if (!handle->stream_flags.is_multi) {
			FILE_IS_CORRUPT();
			return;
		}

		if (parse_metadata(handle))
			return;

	}
}



extern void
list(const char *filename)
{
	if (strcmp(filename, "-") == 0) {
		errmsg(V_ERROR, "%s: --list does not support reading from "
				"standard input", filename);
		return;
	}

	if (is_empty_filename(filename))
		return;

	listing_handle handle;
	handle.filename = filename;

	handle.fd = open(filename, O_RDONLY | O_NOCTTY);
	if (handle.fd == -1) {
		errmsg(V_ERROR, "%s: %s", filename, strerror(errno));
		return;
	}

	if (fstat(handle.fd, &handle.st)) {
		errmsg(V_ERROR, "%s: %s", filename, strerror(errno));
		goto out;
	}

	if (!S_ISREG(handle.st.st_mode)) {
		errmsg(V_WARNING, _("%s: Not a regular file, skipping"),
				filename);
		goto out;
	}

	if (handle.st.st_size <= 0) {
		errmsg(V_ERROR, _("%s: File is empty"), filename);
		goto out;
	}

	if (listing_pread(&handle, 0))
		goto out;

	if (handle.buffer[0] == 0xFF) {
		if (opt_header == HEADER_ALONE) {
			errmsg(V_ERROR, "%s: FIXME", filename); // FIXME
			goto out;
		}

		list_native(&handle);
	} else {
		if (opt_header != HEADER_AUTO && opt_header != HEADER_ALONE) {
			errmsg(V_ERROR, "%s: FIXME", filename); // FIXME
			goto out;
		}

		list_alone(&handle);
	}

out:
	(void)close(fd);
	return;
}
