// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_stream_buffer_encode.c
/// \brief      Tests single-call .xz Stream encoding
///
/// This tests lzma_stream_buffer_bound(), lzma_easy_buffer_encode(), and
/// lzma_stream_buffer_encode(). The single-call decoder
/// (lzma_stream_buffer_decode()) is used to verify that the encoded output
/// decodes back to the original input.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


// Size of the uncompressed test data.
#define DATA_SIZE 4096

// Test data filled in main(). It is referenced unconditionally in main() so
// that it counts as used even in builds where the encoders are disabled.
static uint8_t data[DATA_SIZE];


#if defined(HAVE_ENCODERS) && defined(HAVE_DECODERS)
// Decode the single-call .xz Stream in buf and check that it matches the
// original test data exactly.
static void
verify_decode(const uint8_t *buf, size_t buf_size)
{
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;

	uint8_t out[DATA_SIZE];
	size_t out_pos = 0;

	assert_lzma_ret(lzma_stream_buffer_decode(
			&memlimit, 0, NULL,
			buf, &in_pos, buf_size,
			out, &out_pos, sizeof(out)), LZMA_OK);
	assert_uint_eq(in_pos, buf_size);
	assert_uint_eq(out_pos, DATA_SIZE);
	assert_array_eq(out, data, DATA_SIZE);
}
#endif


static void
test_stream_buffer_bound(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	// Even an empty Stream needs space for the Stream Header,
	// Stream Footer, and Index, so the bound is always nonzero.
	assert_uint(lzma_stream_buffer_bound(0), >, 0);

	// The bound must leave room for the data itself plus the .xz
	// container overhead.
	assert_uint(lzma_stream_buffer_bound(DATA_SIZE), >, DATA_SIZE);

	// SIZE_MAX cannot fit the uncompressed data plus the overhead, so
	// the integer overflow must be detected and reported as zero.
	assert_uint_eq(lzma_stream_buffer_bound(SIZE_MAX), 0);
#endif
}


static void
test_easy_buffer_encode(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	uint8_t out[DATA_SIZE * 2];

	// A successful round trip using a preset.
	size_t out_pos = 0;
	assert_lzma_ret(lzma_easy_buffer_encode(
			LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC32, NULL,
			data, sizeof(data),
			out, &out_pos, sizeof(out)), LZMA_OK);
	assert_uint(out_pos, >, 0);
	assert_uint(out_pos, <=, lzma_stream_buffer_bound(sizeof(data)));

#	ifdef HAVE_DECODERS
	verify_decode(out, out_pos);
#	endif

	// An invalid check value is rejected without touching *out_pos.
	out_pos = 0;
	assert_lzma_ret(lzma_easy_buffer_encode(
			LZMA_PRESET_DEFAULT, INVALID_LZMA_CHECK_ID, NULL,
			data, sizeof(data),
			out, &out_pos, sizeof(out)), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 0);

	// Too little output space results in LZMA_BUF_ERROR and *out_pos is
	// left unchanged.
	uint8_t tiny[1];
	out_pos = 0;
	assert_lzma_ret(lzma_easy_buffer_encode(
			LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC32, NULL,
			data, sizeof(data),
			tiny, &out_pos, sizeof(tiny)), LZMA_BUF_ERROR);
	assert_uint_eq(out_pos, 0);
#endif
}


static void
test_stream_buffer_encode(void)
{
#ifndef HAVE_ENCODERS
	assert_skip("Encoder support is disabled");
#else
	lzma_options_lzma opt_lzma2;
	assert_false(lzma_lzma_preset(&opt_lzma2, LZMA_PRESET_DEFAULT));

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt_lzma2 },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	uint8_t out[DATA_SIZE * 2];

	// A successful round trip using a custom filter chain.
	size_t out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC64, NULL,
			data, sizeof(data),
			out, &out_pos, sizeof(out)), LZMA_OK);
	assert_uint(out_pos, >, 0);
	assert_uint(out_pos, <=, lzma_stream_buffer_bound(sizeof(data)));

#	ifdef HAVE_DECODERS
	verify_decode(out, out_pos);
#	endif

	// An unsupported (but in-range) check type is reported only when the
	// build actually lacks support for it.
	if (!lzma_check_is_supported(LZMA_CHECK_SHA256)) {
		out_pos = 0;
		assert_lzma_ret(lzma_stream_buffer_encode(
				filters, LZMA_CHECK_SHA256, NULL,
				data, sizeof(data),
				out, &out_pos, sizeof(out)),
			LZMA_UNSUPPORTED_CHECK);
		assert_uint_eq(out_pos, 0);
	}

	// A NULL filter chain is a programming error.
	out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_encode(
			NULL, LZMA_CHECK_CRC64, NULL,
			data, sizeof(data),
			out, &out_pos, sizeof(out)), LZMA_PROG_ERROR);
	assert_uint_eq(out_pos, 0);

	// Too little output space results in LZMA_BUF_ERROR and *out_pos is
	// left unchanged.
	uint8_t tiny[1];
	out_pos = 0;
	assert_lzma_ret(lzma_stream_buffer_encode(
			filters, LZMA_CHECK_CRC64, NULL,
			data, sizeof(data),
			tiny, &out_pos, sizeof(tiny)), LZMA_BUF_ERROR);
	assert_uint_eq(out_pos, 0);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);

	// Fill the buffer with semi-repetitive data so that it is
	// compressible but still exercises the encoder.
	for (size_t i = 0; i < DATA_SIZE; ++i)
		data[i] = (uint8_t)((i * 101 + (i >> 3)) & 0xFF);

	tuktest_run(test_stream_buffer_bound);
	tuktest_run(test_easy_buffer_encode);
	tuktest_run(test_stream_buffer_encode);

	return tuktest_end();
}
