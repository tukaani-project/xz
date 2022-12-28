///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_index_hash.c
/// \brief      Tests src/liblzma/common/index_hash.c API functions
///
/// \note       No test included for lzma_index_hash_end since it
///             would be trivial unless tested for memory leaks
///             with something like valgrind
//
//  Author:     Jia Tan
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"
// Needed for UNPADDED_SIZE_MIN and UNPADDED_SIZE_MAX macro definitions
// and index_size and vli_ceil4 helper functions
#include "common/index.h"


static void
test_lzma_index_hash_init(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support disabled");
#else
	// First test with NULL index hash
	// This should create a fresh index hash
	lzma_index_hash *index_hash = lzma_index_hash_init(NULL, NULL);
	assert_true(index_hash != NULL);

	// Next test with non-NULL index hash
	lzma_index_hash *second_hash = lzma_index_hash_init(index_hash, NULL);

	// Should not create a new index_hash pointer
	// Instead must just re-init the first index hash
	assert_true(index_hash == second_hash);

	lzma_index_hash_end(index_hash, NULL);
#endif
}


static void
test_lzma_index_hash_append(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support disabled");
#else
	// Test all invalid parameters
	assert_lzma_ret(lzma_index_hash_append(NULL, 0, 0),
			LZMA_PROG_ERROR);

	// Test NULL index_hash
	assert_lzma_ret(lzma_index_hash_append(NULL, UNPADDED_SIZE_MIN,
			LZMA_VLI_MAX), LZMA_PROG_ERROR);

	// Test with invalid unpadded size
	lzma_index_hash *index_hash = lzma_index_hash_init(NULL, NULL);
	assert_true(index_hash);
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN - 1, LZMA_VLI_MAX),
			LZMA_PROG_ERROR);

	// Test with invalid uncompressed size
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN, LZMA_VLI_MAX + 1),
			LZMA_PROG_ERROR);

	// Append first a small "block" to the index, which should succeed
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	// Append another small "block"
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	// Append a block that would cause the compressed size to grow
	// too big
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MAX, 1), LZMA_DATA_ERROR);

	lzma_index_hash_end(index_hash, NULL);
#endif
}

#ifdef HAVE_DECODERS
// Fill an index_hash with unpadded and uncompressed VLIs
// by calling lzma_index_hash_append
static void
fill_index_hash(lzma_index_hash *index_hash, const lzma_vli *unpadded_sizes,
		const lzma_vli *uncomp_sizes, uint32_t block_count)
{
	for(uint32_t i = 0; i < block_count; i++)
		assert_lzma_ret(lzma_index_hash_append(index_hash,
			unpadded_sizes[i], uncomp_sizes[i]), LZMA_OK);
}


#ifdef HAVE_ENCODERS
// Set the index parameter to the expected index based on the
// xz specification. Needs the unpadded and uncompressed VLIs
// to correctly create the index
static void
generate_index(uint8_t *index, const lzma_vli *unpadded_sizes,
		const lzma_vli *uncomp_sizes, uint32_t block_count,
		size_t index_max_size)
{
	size_t in_pos = 0;
	size_t out_pos = 0;
	// First set index indicator
	index[out_pos++] = 0;

	// Next write out Number of Records
	assert_lzma_ret(lzma_vli_encode(block_count, &in_pos, index,
			&out_pos, index_max_size), LZMA_STREAM_END);

	// Next write out each record
	// A record consists of unpadded size and uncompressed size
	// written next to each other as VLIs
	for (uint32_t i = 0; i < block_count; i++) {
		in_pos = 0;
		assert_lzma_ret(lzma_vli_encode(unpadded_sizes[i], &in_pos,
			index, &out_pos, index_max_size), LZMA_STREAM_END);
		in_pos = 0;
		assert_lzma_ret(lzma_vli_encode(uncomp_sizes[i], &in_pos,
			index, &out_pos, index_max_size), LZMA_STREAM_END);
	}

	// Add index padding
	lzma_vli rounded_out_pos = vli_ceil4(out_pos);
	memzero(index + out_pos, rounded_out_pos - out_pos);
	out_pos = rounded_out_pos;

	// Add the CRC32
	write32le(index + out_pos, lzma_crc32(index, out_pos, 0));
}
#endif
#endif


static void
test_lzma_index_hash_decode(void)
{
#if !defined(HAVE_ENCODERS) || !defined(HAVE_DECODERS)
	assert_skip("Encoder or decoder support disabled");
#else
	lzma_index_hash *index_hash = lzma_index_hash_init(NULL, NULL);
	assert_true(index_hash);

	size_t in_pos = 0;

	// Six valid sizes for unpadded data sizes
	const lzma_vli unpadded_sizes[6] = {
		UNPADDED_SIZE_MIN,
		1000,
		4000,
		8000,
		16000,
		32000
	};

	// Six valid sizes for uncompressed data sizes
	const lzma_vli uncomp_sizes[6] = {
		1,
		500,
		8000,
		20,
		1,
		500
	};

	// Add two entries to a index hash
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 2);

	const lzma_vli size_two_entries = lzma_index_hash_size(index_hash);
	assert_uint(size_two_entries, >, 0);
	uint8_t *index_two_entries = tuktest_malloc(size_two_entries);

	generate_index(index_two_entries, unpadded_sizes, uncomp_sizes, 2,
			size_two_entries);

	// First test for basic buffer size error
	in_pos = size_two_entries + 1;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_two_entries, &in_pos,
			size_two_entries), LZMA_BUF_ERROR);

	// Next test for invalid index indicator
	in_pos = 0;
	index_two_entries[0] ^= 1;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_two_entries, &in_pos,
			size_two_entries), LZMA_DATA_ERROR);
	index_two_entries[0] ^= 1;

	// Next verify the index_hash as expected
	in_pos = 0;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_two_entries, &in_pos,
			size_two_entries), LZMA_STREAM_END);

	// Next test a three entry index hash
	index_hash = lzma_index_hash_init(index_hash, NULL);
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 3);

	const lzma_vli size_three_entries = lzma_index_hash_size(
			index_hash);
	assert_uint(size_three_entries, >, 0);
	uint8_t *index_three_entries = tuktest_malloc(size_three_entries);

	generate_index(index_three_entries, unpadded_sizes, uncomp_sizes,
			3, size_three_entries);

	in_pos = 0;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_three_entries, &in_pos,
			size_three_entries), LZMA_STREAM_END);

	// Next test a five entry index hash
	index_hash = lzma_index_hash_init(index_hash, NULL);
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 5);

	const lzma_vli size_five_entries = lzma_index_hash_size(
			index_hash);
	assert_uint(size_five_entries, >, 0);
	uint8_t *index_five_entries = tuktest_malloc(size_five_entries);

	generate_index(index_five_entries, unpadded_sizes, uncomp_sizes, 5,
			size_five_entries);

	// Instead of testing all input at once, give input
	// one byte at a time
	in_pos = 0;
	for (lzma_vli i = 0; i < size_five_entries - 1; i++) {
		assert_lzma_ret(lzma_index_hash_decode(index_hash,
				index_five_entries, &in_pos, in_pos + 1),
				LZMA_OK);
	}

	// Last byte should return LZMA_STREAM_END
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_five_entries, &in_pos,
			in_pos + 1), LZMA_STREAM_END);

	// Next test if the index hash is given an incorrect unpadded
	// size. Should detect and report LZMA_DATA_ERROR
	index_hash = lzma_index_hash_init(index_hash, NULL);
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 5);
	// The sixth entry will have invalid unpadded size
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			unpadded_sizes[5] + 1,
			uncomp_sizes[5]), LZMA_OK);

	const lzma_vli size_six_entries = lzma_index_hash_size(
			index_hash);

	assert_uint(size_six_entries, >, 0);
	uint8_t *index_six_entries = tuktest_malloc(size_six_entries);

	generate_index(index_six_entries, unpadded_sizes, uncomp_sizes, 6,
			size_six_entries);
	in_pos = 0;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_six_entries, &in_pos,
			size_six_entries), LZMA_DATA_ERROR);

	// Next test if the index is corrupt (invalid CRC)
	// Should detect and report LZMA_DATA_ERROR
	index_hash = lzma_index_hash_init(index_hash, NULL);
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 2);

	index_two_entries[size_two_entries - 1] ^= 1;

	in_pos = 0;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_two_entries, &in_pos,
			size_two_entries), LZMA_DATA_ERROR);

	// Next test with index and index_hash struct not matching
	// an entry
	index_hash = lzma_index_hash_init(index_hash, NULL);
	fill_index_hash(index_hash, unpadded_sizes, uncomp_sizes, 2);
	// Recalculate index with invalid unpadded size
	const lzma_vli unpadded_sizes_invalid[2] = {
		unpadded_sizes[0],
		unpadded_sizes[1] + 1
	};

	generate_index(index_two_entries, unpadded_sizes_invalid,
			uncomp_sizes, 2, size_two_entries);

	in_pos = 0;
	assert_lzma_ret(lzma_index_hash_decode(index_hash,
			index_two_entries, &in_pos,
			size_two_entries), LZMA_DATA_ERROR);

	lzma_index_hash_end(index_hash, NULL);
#endif
}


static void
test_lzma_index_hash_size(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support disabled");
#else
	lzma_index_hash *index_hash = lzma_index_hash_init(NULL, NULL);
	assert_true(index_hash);

	// First test empty index hash
	// Expected size should be:
	// Index Indicator - 1 byte
	// Number of Records - 1 byte
	// List of Records - 0 bytes
	// Index Padding - 2 bytes
	// CRC32 - 4 bytes
	// Total - 8 bytes
	assert_uint_eq(lzma_index_hash_size(index_hash), 8);

	// Append a small block to the index hash
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	// Expected size should be:
	// Index Indicator - 1 byte
	// Number of Records - 1 byte
	// List of Records - 2 bytes
	// Index Padding - 0 bytes
	// CRC32 - 4 bytes
	// Total - 8 bytes
	lzma_vli expected_size = 8;
	assert_uint_eq(lzma_index_hash_size(index_hash), expected_size);

	// Append additional small block
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	// Expected size should be:
	// Index Indicator - 1 byte
	// Number of Records - 1 byte
	// List of Records - 4 bytes
	// Index Padding - 2 bytes
	// CRC32 - 4 bytes
	// Total - 12 bytes
	expected_size = 12;
	assert_uint_eq(lzma_index_hash_size(index_hash), expected_size);

	// Append a larger block to the index hash (3 bytes for each vli)
	const lzma_vli three_byte_vli = 0x10000;
	assert_lzma_ret(lzma_index_hash_append(index_hash,
			three_byte_vli, three_byte_vli), LZMA_OK);

	// Expected size should be:
	// Index Indicator - 1 byte
	// Number of Records - 1 byte
	// List of Records - 10 bytes
	// Index Padding - 0 bytes
	// CRC32 - 4 bytes
	// Total - 16 bytes
	expected_size = 16;
	assert_uint_eq(lzma_index_hash_size(index_hash), expected_size);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);
	tuktest_run(test_lzma_index_hash_init);
	tuktest_run(test_lzma_index_hash_append);
	tuktest_run(test_lzma_index_hash_decode);
	tuktest_run(test_lzma_index_hash_size);
	return tuktest_end();
}
