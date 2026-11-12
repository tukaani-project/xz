// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_bcj_alpha.c
/// \brief      Tests the DEC Alpha BCJ filter
//
//  Author:     Matt Turner
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


#define ALPHA_LDA(ra, rb, disp) \
	(((uint32_t)0x08 << 26) | ((uint32_t)(ra) << 21) \
		| ((uint32_t)(rb) << 16) | ((uint32_t)(disp) & 0xFFFF))

#define ALPHA_LDAH(ra, rb, disp) \
	(((uint32_t)0x09 << 26) | ((uint32_t)(ra) << 21) \
		| ((uint32_t)(rb) << 16) | ((uint32_t)(disp) & 0xFFFF))

#define ALPHA_BSR(ra, disp) \
	(((uint32_t)0x34 << 26) | ((uint32_t)(ra) << 21) \
		| ((uint32_t)(disp) & 0x001FFFFF))

#define ALPHA_BR(ra, disp) \
	(((uint32_t)0x30 << 26) | ((uint32_t)(ra) << 21) \
		| ((uint32_t)(disp) & 0x001FFFFF))

#define ALPHA_REG_RA 26
#define ALPHA_REG_PV 27
#define ALPHA_REG_GP 29

// bis $31,$31,$31
#define ALPHA_UNOP UINT32_C(0x47FF041F)

#define TEST_DATA_SIZE (64U << 10)


static uint8_t test_data[TEST_DATA_SIZE];

static lzma_options_lzma opt_lzma2;

static const lzma_filter filters[3] = {
	{ .id = LZMA_FILTER_ALPHA, .options = NULL },
	{ .id = LZMA_FILTER_LZMA2, .options = &opt_lzma2 },
	{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
};


/// Fill test_data[] with pseudo-random GP displacement pairs, BSRs, BRs,
/// and other instructions.
static void
create_test_data(void)
{
	uint32_t rng = 0x12345678;
	size_t pos = 0;

	while (pos + 4 <= TEST_DATA_SIZE) {
		// xorshift32
		rng ^= rng << 13;
		rng ^= rng >> 17;
		rng ^= rng << 5;

		const size_t gap = 1 + (rng >> 8) % 4;

		if (rng % 4 == 0 && pos + (gap + 1) * 4 <= TEST_DATA_SIZE) {
			static const uint32_t bases[] = {
				ALPHA_REG_RA, ALPHA_REG_PV, ALPHA_REG_GP,
			};
			const uint32_t base = bases[(rng >> 16) % 3];

			write32le(test_data + pos,
					ALPHA_LDAH(ALPHA_REG_GP, base,
						(rng >> 4) & 0xFFFF));

			for (size_t i = 1; i < gap; ++i)
				write32le(test_data + pos + i * 4,
						ALPHA_UNOP);

			write32le(test_data + pos + gap * 4,
					ALPHA_LDA(ALPHA_REG_GP, ALPHA_REG_GP,
						(rng >> 12) & 0xFFFF));

			pos += (gap + 1) * 4;
			continue;
		}

		uint32_t instr;

		if (rng % 4 == 1) {
			// Mostly the registers that are converted,
			// sometimes any register.
			const uint32_t ra = (rng >> 29) == 0
					? (rng >> 24) & 0x1F
					: (rng >> 28) & 1 ? 31 : ALPHA_REG_RA;
			instr = (rng >> 27) & 1
					? ALPHA_BR(ra, rng >> 3)
					: ALPHA_BSR(ra, rng >> 3);
		} else if (rng % 4 == 2) {
			instr = ALPHA_LDAH(ALPHA_REG_GP, 1, rng >> 5);
		} else {
			instr = ALPHA_UNOP + (rng & 0xFF);
		}

		write32le(test_data + pos, instr);
		pos += 4;
	}
}


/// Run strm over in[] with input and output given chunk_size bytes at
/// a time. Returns the output size.
static size_t
code_chunked(lzma_stream *strm, size_t chunk_size,
		const uint8_t *in, size_t in_size,
		uint8_t *out, size_t out_size)
{
	strm->next_in = in;
	strm->avail_in = 0;
	strm->next_out = out;
	strm->avail_out = 0;

	size_t in_left = in_size;
	size_t out_left = out_size;
	lzma_ret ret;

	do {
		if (strm->avail_in == 0 && in_left > 0) {
			strm->avail_in = my_min(chunk_size, in_left);
			in_left -= strm->avail_in;
		}

		if (strm->avail_out == 0 && out_left > 0) {
			strm->avail_out = my_min(chunk_size, out_left);
			out_left -= strm->avail_out;
		}

		ret = lzma_code(strm, in_left == 0 ? LZMA_FINISH : LZMA_RUN);
	} while (ret == LZMA_OK);

	assert_lzma_ret(ret, LZMA_STREAM_END);

	return out_size - out_left - strm->avail_out;
}


static void
test_chunking(void)
{
	if (!lzma_filter_encoder_is_supported(LZMA_FILTER_ALPHA)
			|| !lzma_filter_decoder_is_supported(
				LZMA_FILTER_ALPHA))
		assert_skip("DEC Alpha BCJ encoder and/or decoder "
				"is disabled");

	static const size_t chunk_sizes[] = {
		1, 3, 4, 5, 7, 16, 19, 20, 21, 40, 41, 4096, 65536,
	};

	const size_t buf_size = sizeof(test_data) + 4096;
	uint8_t *ref = tuktest_malloc(buf_size);
	uint8_t *buf = tuktest_malloc(buf_size);

	lzma_stream strm = LZMA_STREAM_INIT;
	assert_lzma_ret(lzma_raw_encoder(&strm, filters), LZMA_OK);
	const size_t ref_size = code_chunked(&strm, sizeof(test_data),
			test_data, sizeof(test_data), ref, buf_size);

	for (size_t i = 0; i < ARRAY_SIZE(chunk_sizes); ++i) {
		assert_lzma_ret(lzma_raw_encoder(&strm, filters), LZMA_OK);
		assert_uint_eq(code_chunked(&strm, chunk_sizes[i],
				test_data, sizeof(test_data),
				buf, buf_size), ref_size);
		assert_array_eq(buf, ref, ref_size);

		assert_lzma_ret(lzma_raw_decoder(&strm, filters), LZMA_OK);
		assert_uint_eq(code_chunked(&strm, chunk_sizes[i],
				ref, ref_size, buf, sizeof(test_data)),
				sizeof(test_data));
		assert_array_eq(buf, test_data, sizeof(test_data));
	}

	lzma_end(&strm);
}


/// Inputs around the filter's 20-byte window
static void
test_short_input(void)
{
	if (!lzma_filter_encoder_is_supported(LZMA_FILTER_ALPHA)
			|| !lzma_filter_decoder_is_supported(
				LZMA_FILTER_ALPHA))
		assert_skip("DEC Alpha BCJ encoder and/or decoder "
				"is disabled");

	uint8_t compressed[1024];
	uint8_t out[64];

	for (size_t size = 0; size <= 32; ++size) {
		size_t compressed_pos = 0;
		assert_lzma_ret(lzma_raw_buffer_encode(filters, NULL,
				test_data, size, compressed,
				&compressed_pos, sizeof(compressed)),
			LZMA_OK);

		size_t in_pos = 0;
		size_t out_pos = 0;
		assert_lzma_ret(lzma_raw_buffer_decode(filters, NULL,
				compressed, &in_pos, compressed_pos,
				out, &out_pos, sizeof(out)),
			LZMA_OK);

		assert_uint_eq(out_pos, size);
		assert_array_eq(out, test_data, size);
	}
}


static void
test_raw_functions(void)
{
#if !defined(HAVE_ENCODER_ALPHA) || !defined(HAVE_DECODER_ALPHA)
	assert_skip("DEC Alpha BCJ encoder and/or decoder is disabled");
#else
	uint8_t *buf = tuktest_malloc(sizeof(test_data));

	for (size_t size = 0; size < 20; ++size)
		assert_uint_eq(lzma_bcj_alpha_encode(0, buf, size), 0);

	static const uint32_t offsets[] = { 0, 4, 4096, UINT32_MAX - 3 };

	for (size_t i = 0; i < ARRAY_SIZE(offsets); ++i) {
		memcpy(buf, test_data, sizeof(test_data));

		const size_t enc = lzma_bcj_alpha_encode(
				offsets[i], buf, sizeof(test_data));
		assert_uint_eq(enc % 4, 0);
		assert_uint(sizeof(test_data) - enc, <, 20);

		assert_uint_eq(lzma_bcj_alpha_decode(
				offsets[i], buf, sizeof(test_data)), enc);
		assert_array_eq(buf, test_data, sizeof(test_data));
	}
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);

#if !defined(HAVE_ENCODERS) || !defined(HAVE_DECODERS)
	tuktest_early_skip("Encoder or decoder support disabled");
#else
	if (lzma_lzma_preset(&opt_lzma2, 0))
		tuktest_error("lzma_lzma_preset() failed");

	create_test_data();
	tuktest_run(test_chunking);
	tuktest_run(test_short_input);
	tuktest_run(test_raw_functions);
#endif

	return tuktest_end();
}
