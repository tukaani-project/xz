// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_stream_buffer_decode.c
/// \brief      Tests lzma_stream_buffer_decode()
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


// Uncompressed size of the test file
#define UNCOMP_SIZE 13

// Test file data and its compressed size
static uint8_t *xz_data;
static size_t xz_data_size;


static void
test_success(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support is disabled");
#else
	uint64_t memlimit = 1 << 20;

	size_t in_pos = 0;

	uint8_t out[UNCOMP_SIZE];
	size_t out_pos = 0;
	size_t out_size = sizeof(out);

	assert_lzma_ret(lzma_stream_buffer_decode(
			&memlimit, LZMA_CONCATENATED, NULL,
			xz_data, &in_pos, xz_data_size,
			out, &out_pos, out_size), LZMA_OK);
	assert_uint_eq(in_pos, xz_data_size);
	assert_uint_eq(out_pos, UNCOMP_SIZE);
#endif
}


static void
test_data_error(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support is disabled");
#else
	uint64_t memlimit = 1 << 20;

	size_t in_pos = 0;

	uint8_t out[UNCOMP_SIZE];
	size_t out_pos = 0;
	size_t out_size = sizeof(out);

	// This fails in 5.8.3 and older.
	assert_lzma_ret(lzma_stream_buffer_decode(
			&memlimit, LZMA_CONCATENATED, NULL,
			xz_data, &in_pos, xz_data_size - 1,
			out, &out_pos, out_size), LZMA_DATA_ERROR);
	assert_uint_eq(in_pos, 0);
	assert_uint_eq(out_pos, 0);
#endif
}


static void
test_buf_error(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support is disabled");
#else
	uint64_t memlimit = 1 << 20;

	size_t in_pos = 0;

	uint8_t out[UNCOMP_SIZE - 1];
	size_t out_pos = 0;
	size_t out_size = sizeof(out);

	assert_lzma_ret(lzma_stream_buffer_decode(
			&memlimit, LZMA_CONCATENATED, NULL,
			xz_data, &in_pos, xz_data_size,
			out, &out_pos, out_size), LZMA_BUF_ERROR);
	assert_uint_eq(in_pos, 0);
	assert_uint_eq(out_pos, 0);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);

	xz_data = tuktest_file_from_srcdir("files/good-1-check-crc32.xz",
			&xz_data_size);

	tuktest_run(test_success);
	tuktest_run(test_data_error);
	tuktest_run(test_buf_error);

	return tuktest_end();
}
