// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_raw.c
/// \brief      Test raw encoding and decoding
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


static const uint8_t preset_dict[] = { 1, 2, 3, 4, 1, 2, 3, 4 };

static uint8_t in[16] = { 1, 2, 3, 4,  1, 2, 3, 4,
			  9, 0, 0, 0,  0, 0, 0, 0 };
static uint8_t encoded[128];
static uint8_t decoded[16];


// Before the commit
// "liblzma: Silence Valgrind and -fsanitize=memory with a preset dictionary":
// This can fail with -fsanitize=memory or with Valgrind due to the use of
// an uninitialized value. The uninitialized value was eventually ignored,
// so the sanitizer and Valgrind errors were the only real problem.
static void
test_preset_dict(void)
{
	lzma_options_lzma opt_lzma2;
	assert_false(lzma_lzma_preset(&opt_lzma2, 4));

	opt_lzma2.dict_size = 4096;
	opt_lzma2.nice_len = 4;

	opt_lzma2.preset_dict = preset_dict;
	opt_lzma2.preset_dict_size = sizeof(preset_dict);

	lzma_filter filters[] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt_lzma2 },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	size_t enc_size = 0;
	assert_lzma_ret(lzma_raw_buffer_encode(filters, NULL, in, sizeof(in),
			encoded, &enc_size, sizeof(encoded)), LZMA_OK);
	assert_uint(enc_size, >, 0);
	assert_uint(enc_size, <=, sizeof(encoded));

	size_t in_pos = 0;
	size_t dec_size = 0;
	assert_lzma_ret(lzma_raw_buffer_decode(filters, NULL,
			encoded, &in_pos, enc_size,
			decoded, &dec_size, sizeof(decoded)), LZMA_OK);
	assert_uint_eq(in_pos, enc_size);
	assert_uint_eq(dec_size, sizeof(in));
	assert_array_eq(in, decoded, sizeof(in));

	// Decoding without a preset dictionary will fail because the raw
	// LZMA2 stream doesn't start with a dictionary reset.
	opt_lzma2.preset_dict = NULL;
	opt_lzma2.preset_dict_size = 0;
	in_pos = 0;
	dec_size = 0;
	assert_lzma_ret(lzma_raw_buffer_decode(filters, NULL,
			encoded, &in_pos, enc_size,
			decoded, &dec_size, sizeof(decoded)), LZMA_DATA_ERROR);
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);
	tuktest_run(test_preset_dict);
	return tuktest_end();
}
