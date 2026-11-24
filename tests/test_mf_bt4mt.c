// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_mf_bt4mt.c
/// \brief      Tests that LZMA_MF_BT4MT gives the same output as LZMA_MF_BT4
//
//  Author:     Noelle
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"
#include "mythread.h"


#if defined(HAVE_ENCODER_LZMA2) && defined(HAVE_MF_BT4)
#define INPUT_SIZE_MAX (768 * 1024)

/// Input size for most tests. nice_len = 273 is slow with the long runs
/// of one byte, so those tests use less input.
#define SIZE (256 * 1024)
#define SIZE_NICE_273 (96 * 1024)

static uint8_t *input;
static uint32_t rng_state;


static uint32_t
rng(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}


/// Fill input[] with data that has matches of many lengths and distances,
/// some long runs of one byte, and some incompressible parts.
static void
create_input(void)
{
	input = tuktest_malloc(INPUT_SIZE_MAX);
	rng_state = 0x12345678;

	size_t pos = 0;
	while (pos < INPUT_SIZE_MAX) {
		const uint32_t r = rng();
		size_t len = 2 + r % 400;
		if (len > INPUT_SIZE_MAX - pos)
			len = INPUT_SIZE_MAX - pos;

		switch ((r >> 16) % 8) {
		case 0:
			for (size_t i = 0; i < len; ++i)
				input[pos + i] = (uint8_t)rng();
			break;

		case 1:
			memset(input + pos, (int)(r >> 24), len);
			break;

		default:
			if (pos < len) {
				for (size_t i = 0; i < len; ++i)
					input[pos + i] = (uint8_t)('a'
						+ rng() % 4);
			} else {
				const size_t dist = 1 + rng() % (pos - len + 1);
				memmove(input + pos, input + pos - dist, len);
			}

			break;
		}

		pos += len;
	}

	return;
}


enum coder_type {
	CODER_STREAM,
	CODER_RAW,
	CODER_ALONE,
	CODER_MT,
};


/// Encode in_size bytes of input[], repeated if needed, in pieces of
/// varying size with occasional flushes and small output buffers.
/// The sequence of calls depends only on seed.
static size_t
encode(enum coder_type type, lzma_options_lzma *opt, size_t in_size,
		uint32_t seed, uint8_t *out, size_t out_size)
{
	lzma_filter filters[2] = {
		{ .id = type == CODER_ALONE ? LZMA_FILTER_LZMA1
				: LZMA_FILTER_LZMA2,
		  .options = opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_stream strm = LZMA_STREAM_INIT;

	switch (type) {
	case CODER_STREAM:
		assert_lzma_ret(lzma_stream_encoder(&strm, filters,
				LZMA_CHECK_CRC32), LZMA_OK);
		break;

	case CODER_RAW:
		assert_lzma_ret(lzma_raw_encoder(&strm, filters), LZMA_OK);
		break;

	case CODER_ALONE:
		assert_lzma_ret(lzma_alone_encoder(&strm, opt), LZMA_OK);
		break;

	case CODER_MT: {
#ifdef MYTHREAD_ENABLED
		const lzma_mt mt = {
			.threads = 3,
			.block_size = 100 * 1024,
			.filters = filters,
			.check = LZMA_CHECK_CRC32,
		};
		assert_lzma_ret(lzma_stream_encoder_mt(&strm, &mt), LZMA_OK);
#else
		assert_error("CODER_MT without threading");
#endif
		break;
	}
	}

	rng_state = seed;

	size_t in_pos = 0;
	size_t out_pos = 0;

	while (true) {
		const uint32_t in_max = rng() % 2 ? 100 : 100000;
		size_t in_avail = 1 + rng() % in_max;
		if (in_avail > in_size - in_pos)
			in_avail = in_size - in_pos;

		const size_t in_off = in_pos % INPUT_SIZE_MAX;
		if (in_avail > INPUT_SIZE_MAX - in_off)
			in_avail = INPUT_SIZE_MAX - in_off;

		lzma_action action = LZMA_RUN;
		const uint32_t r = rng() % 16;

		if (in_pos + in_avail == in_size)
			action = LZMA_FINISH;
		else if ((type == CODER_STREAM || type == CODER_RAW)
				&& r == 0)
			action = LZMA_SYNC_FLUSH;
		else if (type == CODER_STREAM && r == 1)
			action = LZMA_FULL_FLUSH;

		strm.next_in = input + in_off;
		strm.avail_in = in_avail;

		lzma_ret ret;
		do {
			size_t out_avail = 1 + rng() % 5000;
			if (out_avail > out_size - out_pos)
				out_avail = out_size - out_pos;

			assert_true(out_avail > 0);

			strm.next_out = out + out_pos;
			strm.avail_out = out_avail;

			ret = lzma_code(&strm, action);
			assert_true(ret == LZMA_OK || ret == LZMA_STREAM_END);

			out_pos += out_avail - strm.avail_out;

		} while (action == LZMA_RUN ? strm.avail_in > 0
				: ret != LZMA_STREAM_END);

		in_pos += in_avail;
		if (action == LZMA_FINISH)
			break;
	}

	lzma_end(&strm);
	return out_pos;
}


static void
make_opt(lzma_options_lzma *opt, uint32_t preset, uint32_t dict_size,
		uint32_t nice_len, uint32_t depth)
{
	assert_false(lzma_lzma_preset(opt, preset));
	opt->dict_size = dict_size;
	opt->nice_len = nice_len;
	opt->depth = depth;
	return;
}


/// Encode with LZMA_MF_BT4 and LZMA_MF_BT4MT and compare the output.
static void
compare(enum coder_type type, const lzma_options_lzma *base, size_t in_size)
{
	lzma_options_lzma opt = *base;
	const size_t out_size = in_size + in_size / 2 + 1024;
	uint8_t *out_bt4 = tuktest_malloc(out_size);
	uint8_t *out_bt4mt = tuktest_malloc(out_size);

	for (uint32_t seed = 1; seed <= 2; ++seed) {
		opt.mf = LZMA_MF_BT4;
		const size_t size_bt4 = encode(type, &opt, in_size, seed,
				out_bt4, out_size);

		opt.mf = LZMA_MF_BT4MT;
		const size_t size_bt4mt = encode(type, &opt, in_size, seed,
				out_bt4mt, out_size);

		assert_uint_eq(size_bt4, size_bt4mt);
		assert_array_eq(out_bt4, out_bt4mt, size_bt4);
	}

	tuktest_free(out_bt4);
	tuktest_free(out_bt4mt);
	return;
}


static void
test_stream(void)
{
	lzma_options_lzma opt;

	make_opt(&opt, 6, 1 << 20, 64, 0);
	compare(CODER_STREAM, &opt, SIZE);

	make_opt(&opt, 6, 1 << 20, 4, 0);
	compare(CODER_STREAM, &opt, SIZE);

	make_opt(&opt, 6, 1 << 20, 273, 0);
	compare(CODER_STREAM, &opt, SIZE_NICE_273);

	make_opt(&opt, 6 | LZMA_PRESET_EXTREME, 1 << 20, 273, 512);
	compare(CODER_STREAM, &opt, SIZE_NICE_273);

	// The binary tree wraps around and the depth limit is reached.
	make_opt(&opt, 6, 64 << 10, 64, 4);
	compare(CODER_STREAM, &opt, SIZE);

	make_opt(&opt, 6, 1 << 20, 64, 0);
	opt.mode = LZMA_MODE_FAST;
	compare(CODER_STREAM, &opt, SIZE);
}


static void
test_short(void)
{
	static const size_t sizes[] = { 0, 1, 4, 20, 100, 300, 5000 };
	lzma_options_lzma opt;
	make_opt(&opt, 6, 1 << 20, 273, 0);

	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i)
		compare(CODER_STREAM, &opt, sizes[i]);
}


static void
test_raw_preset_dict(void)
{
	lzma_options_lzma opt;
	make_opt(&opt, 6, 1 << 20, 64, 0);
	opt.preset_dict = input + INPUT_SIZE_MAX / 2;
	opt.preset_dict_size = 64 * 1024;
	compare(CODER_RAW, &opt, SIZE);

	opt.nice_len = 273;
	compare(CODER_RAW, &opt, SIZE_NICE_273);
}


static void
test_alone(void)
{
	lzma_options_lzma opt;
	make_opt(&opt, 6, 1 << 20, 64, 0);
	compare(CODER_ALONE, &opt, SIZE);

	// LZMA1 has no chunk buffer, so with a small dictionary the
	// input window is moved about every 512 KiB.
	make_opt(&opt, 6, LZMA_DICT_SIZE_MIN, 32, 0);
	compare(CODER_ALONE, &opt, 4 * INPUT_SIZE_MAX);
}


static void
test_stream_encoder_mt(void)
{
#ifndef MYTHREAD_ENABLED
	assert_skip("Threading support disabled");
#else
	lzma_options_lzma opt;
	make_opt(&opt, 6, 1 << 20, 64, 0);
	compare(CODER_MT, &opt, SIZE);
#endif
}


/// Encode in_size bytes of input[] in one go. If update_mf isn't zero,
/// the match finder is changed to it in the middle of the stream.
static size_t
encode_simple(lzma_stream *strm, lzma_options_lzma *opt,
		lzma_match_finder update_mf, size_t in_size,
		uint8_t *out, size_t out_size)
{
	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	assert_lzma_ret(lzma_stream_encoder(strm, filters, LZMA_CHECK_CRC32),
			LZMA_OK);

	strm->next_in = input;
	strm->next_out = out;
	strm->avail_out = out_size;

	if (update_mf != 0) {
		strm->avail_in = in_size / 2;
		assert_lzma_ret(lzma_code(strm, LZMA_FULL_BARRIER),
				LZMA_STREAM_END);

		opt->mf = update_mf;
		assert_lzma_ret(lzma_filters_update(strm, filters), LZMA_OK);
	}

	strm->avail_in = in_size - (size_t)(strm->next_in - input);
	assert_lzma_ret(lzma_code(strm, LZMA_FINISH), LZMA_STREAM_END);

	return out_size - strm->avail_out;
}


static void
test_reinit(void)
{
	const size_t in_size = SIZE;
	const size_t out_size = in_size + in_size / 2;
	uint8_t *ref = tuktest_malloc(out_size);
	uint8_t *out = tuktest_malloc(out_size);
	lzma_options_lzma opt;
	make_opt(&opt, 6, 1 << 20, 64, 0);

	lzma_stream strm = LZMA_STREAM_INIT;
	opt.mf = LZMA_MF_BT4;
	const size_t ref_size = encode_simple(&strm, &opt, 0, in_size,
			ref, out_size);

	// Reinitialize the same lzma_stream with different match finders.
	static const lzma_match_finder mfs[] = {
		LZMA_MF_BT4MT, LZMA_MF_BT4, LZMA_MF_BT4MT, LZMA_MF_BT4MT,
	};

	for (size_t i = 0; i < ARRAY_SIZE(mfs); ++i) {
		opt.mf = mfs[i];
		assert_uint_eq(encode_simple(&strm, &opt, 0, in_size,
				out, out_size), ref_size);
		assert_array_eq(out, ref, ref_size);
	}

	// Change the match finder in the middle of a stream.
	opt.mf = LZMA_MF_BT4;
	const size_t ref2_size = encode_simple(&strm, &opt, LZMA_MF_BT4,
			in_size, ref, out_size);

	opt.mf = LZMA_MF_BT4MT;
	assert_uint_eq(encode_simple(&strm, &opt, LZMA_MF_BT4, in_size,
			out, out_size), ref2_size);
	assert_array_eq(out, ref, ref2_size);

	opt.mf = LZMA_MF_BT4;
	assert_uint_eq(encode_simple(&strm, &opt, LZMA_MF_BT4MT, in_size,
			out, out_size), ref2_size);
	assert_array_eq(out, ref, ref2_size);

	lzma_end(&strm);
	tuktest_free(ref);
	tuktest_free(out);
}


static void
test_memusage(void)
{
	lzma_options_lzma opt;
	assert_false(lzma_lzma_preset(&opt, 6));

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	opt.mf = LZMA_MF_BT4;
	const uint64_t bt4 = lzma_raw_encoder_memusage(filters);

	opt.mf = LZMA_MF_BT4MT;
	const uint64_t bt4mt = lzma_raw_encoder_memusage(filters);

	assert_true(bt4 != UINT64_MAX);
	assert_true(bt4mt != UINT64_MAX);
	assert_true(bt4mt > bt4);
}
#endif


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);

#if !defined(HAVE_ENCODER_LZMA2) || !defined(HAVE_MF_BT4)
	tuktest_early_skip("LZMA2 encoder or BT4 match finder disabled");
#else
	if (!lzma_mf_is_supported(LZMA_MF_BT4MT))
		tuktest_early_skip("LZMA_MF_BT4MT is not supported");

	create_input();

	tuktest_run(test_stream);
	tuktest_run(test_short);
	tuktest_run(test_raw_preset_dict);
	tuktest_run(test_alone);
	tuktest_run(test_stream_encoder_mt);
	tuktest_run(test_reinit);
	tuktest_run(test_memusage);
#endif

	return tuktest_end();
}
