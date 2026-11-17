// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_stream_buffer_encode.c
/// \brief      Test single-call .xz encoding
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


/// LZMA2 preset to use
#define PRESET 1

/// Memory usage limit for decompression
#define MEMLIMIT (2U << 20)


#ifdef HAVE_ENCODERS
/// Compressible data (also with delta:dist=1)
static const uint8_t compressible[]
		= { 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4 };

// Incompressible data (also with delta:dist=1)
static const uint8_t incompressible[] = { 1, 3, 7 };
#endif


static void
test_stream_buffer_bound(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	size_t bound;

	bound = lzma_stream_buffer_bound(0);
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(bound, <, 222);

	bound = lzma_stream_buffer_bound(123);
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE + 123);
	assert_uint(bound, <, 333);

	bound = lzma_stream_buffer_bound(SIZE_MAX - 123);
	assert_uint_eq(bound, 0);

	bound = lzma_stream_buffer_bound(SIZE_MAX);
	assert_uint_eq(bound, 0);
#endif
}


static void
test_easy_encoder_memusage(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	// Invalid preset. This fails with liblzma <= 5.8.4.
	assert_uint_eq(lzma_easy_encoder_memusage(7777), UINT64_MAX);

	uint64_t memusage = lzma_easy_encoder_memusage(
			6 | LZMA_PRESET_EXTREME);
	assert_uint(memusage, >, UINT64_C(80) << 20);
	assert_uint(memusage, <, UINT64_C(110) << 20);
#endif
}


#ifdef HAVE_ENCODERS
static void
decompress_and_compare(const uint8_t *in, size_t in_size,
		const uint8_t *uncomp, size_t uncomp_size)
{
#ifndef HAVE_DECODERS
	(void)in;
	(void)in_size;
	(void)uncomp;
	(void)uncomp_size;
#else
	uint8_t out[512] = { 0 };
	assert_uint(uncomp_size, <=, sizeof(out));

	uint64_t memlimit = MEMLIMIT;
	size_t in_pos = 0;
	size_t out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_decode(&memlimit, 0, NULL,
			in, &in_pos, in_size, out, &out_pos, sizeof(out)),
			LZMA_OK);
	assert_uint_eq(in_pos, in_size);
	assert_uint_eq(out_pos, uncomp_size);
	assert_array_eq(out, uncomp, uncomp_size);
#endif
}
#endif


static void
test_easy_buffer_encode(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	uint8_t out[512] = { 0 };
	size_t out_pos;
	size_t bound;

	// Test with compressible data.
	bound = lzma_stream_buffer_bound(sizeof(compressible));
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(bound, <, sizeof(out));
	out_pos = 0;
	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			compressible, sizeof(compressible),
			out, &out_pos, bound), LZMA_OK);
	assert_uint(out_pos, >, 2 * LZMA_STREAM_HEADER_SIZE);
	// It's compressible, thus <.
	assert_uint(out_pos, <, bound);
	decompress_and_compare(out, out_pos,
			compressible, sizeof(compressible));

	// Test with incompressible data.
	bound = lzma_stream_buffer_bound(sizeof(incompressible));
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(bound, <, sizeof(out));
	out_pos = 0;
	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_OK);
	assert_uint(out_pos, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(out_pos, <=, bound);
	decompress_and_compare(out, out_pos,
			incompressible, sizeof(incompressible));

	// Test with too little output space.
	bound = out_pos;
	out_pos = 1;
	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_BUF_ERROR);
	assert_uint_eq(out_pos, 1);

	// Test with unsupported or invalid arguments.
	assert_lzma_ret(lzma_easy_buffer_encode(
			555, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_OPTIONS_ERROR);
	assert_uint_eq(out_pos, 1);

	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, INVALID_LZMA_CHECK_ID, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 1);

	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, (lzma_check)15, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_UNSUPPORTED_CHECK);
	assert_uint_eq(out_pos, 1);

	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			NULL, sizeof(incompressible),
			out, &out_pos, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 1);

	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			NULL, &out_pos, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 1);

	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, NULL, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 1);

	out_pos = bound + 1;
	assert_lzma_ret(lzma_easy_buffer_encode(
			PRESET, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, bound + 1);
#endif
}


static void
test_stream_buffer_encode(void)
{
#if !defined(HAVE_ENCODER_DELTA) || !defined(HAVE_DECODER_DELTA)
	assert_skip("Delta encoder or decoder support is disabled");
#else
	uint8_t out[512] = { 0 };
	size_t out_pos;
	size_t bound;

	lzma_options_delta opt_delta = { .dist = 1 };
	lzma_options_lzma opt_lzma2;
	assert_uint_eq(lzma_lzma_preset(&opt_lzma2, PRESET), false);
	lzma_filter filters[] = {
		{ LZMA_FILTER_DELTA, &opt_delta },
		{ LZMA_FILTER_LZMA2, &opt_lzma2 },
		{ LZMA_VLI_UNKNOWN, NULL }
	};

	// Test with compressible data.
	bound = lzma_stream_buffer_bound(sizeof(compressible));
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(bound, <, sizeof(out));
	out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC32, NULL,
			compressible, sizeof(compressible),
			out, &out_pos, bound), LZMA_OK);
	assert_uint(out_pos, >, 2 * LZMA_STREAM_HEADER_SIZE);
	// It's compressible even with the Delta filter, thus <.
	assert_uint(out_pos, <, bound);
	decompress_and_compare(out, out_pos,
			compressible, sizeof(compressible));

	// Test with incompressible data.
	bound = lzma_stream_buffer_bound(sizeof(incompressible));
	assert_uint(bound, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(bound, <, sizeof(out));
	out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_OK);
	assert_uint(out_pos, >, 2 * LZMA_STREAM_HEADER_SIZE);
	assert_uint(out_pos, <=, bound);
	decompress_and_compare(out, out_pos,
			incompressible, sizeof(incompressible));

	// Test with too little output space.
	bound = out_pos;
	out_pos = 1;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_BUF_ERROR);
	assert_uint_eq(out_pos, 1);

	// Test with unsupported or invalid arguments.
	assert_lzma_ret(lzma_stream_buffer_encode(
			NULL, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 1);

	filters[0].id = 5555;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC32, NULL,
			incompressible, sizeof(incompressible),
			out, &out_pos, bound), LZMA_OPTIONS_ERROR);
	assert_uint_eq(out_pos, 1);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);

	tuktest_run(test_stream_buffer_bound);
	tuktest_run(test_easy_encoder_memusage);
	tuktest_run(test_easy_buffer_encode);
	tuktest_run(test_stream_buffer_encode);

	return tuktest_end();
}
