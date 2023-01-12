///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_index.c
/// \brief      Tests functions handling the lzma_index structure
///
/// \todo       Implement tests for lzma_file_info_decoder
//
//  Authors:    Jia Tan
//              Lasse Collin
//
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"

// liblzma internal header file needed for:
// UNPADDED_SIZE_MIN
// UNPADDED_SIZE_MAX
// vli_ceil4
#include "common/index.h"


#define MEMLIMIT (LZMA_VLI_C(1) << 20)

static uint8_t *decode_buffer;
static size_t decode_buffer_size = 0;
static lzma_index *decode_test_index;


static void
test_lzma_index_memusage(void)
{
	// The return value from lzma_index_memusage is an approximation
	// of the amount of memory needed to hold the index meta data for
	// a given amount of streams and blocks. It will be an upperbound,
	// so this test will mostly sanity check and error check the
	// function

	// The maximum number of streams should be UINT32_MAX
	assert_uint_eq(lzma_index_memusage((lzma_vli) UINT32_MAX + 1, 1),
			UINT64_MAX);

	// The maximum number of blocks should be LZMA_VLI_MAX
	assert_uint_eq(lzma_index_memusage(1, LZMA_VLI_MAX), UINT64_MAX);

	// Number of streams must be non zero
	assert_uint_eq(lzma_index_memusage(0, 1), UINT64_MAX);

	// Number of blocks CAN be zero
	assert_uint(lzma_index_memusage(1, 0), !=, UINT64_MAX);

	// Arbitrary values for stream and block should work without error
	// and should always increase
	uint64_t previous = 1;
	lzma_vli streams = 1;
	lzma_vli blocks = 1;
	// Test 100 different increasing values for streams and block
	for (int i = 0; i < 100; i++) {
		uint64_t current = lzma_index_memusage(streams, blocks);
		assert_uint(current, >, previous);
		previous = current;
		streams += 29;
		blocks += 107;
	}

	// Force integer overflow in calculation (should result in error)
	assert_uint_eq(lzma_index_memusage(UINT32_MAX, LZMA_VLI_MAX),
			UINT64_MAX);
}


static void
test_lzma_index_memused(void)
{
	// Very similar to test_lzma_index_memusage above since
	// lzma_index_memused is essentially a wrapper for
	// lzma_index_memusage
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Should pass an empty index since initialization creates
	// 1 stream
	assert_uint(lzma_index_memused(idx), <, UINT64_MAX);

	// Append small blocks and then test again (should pass)
	for (int i = 0; i < 10; i++)
		assert_lzma_ret(lzma_index_append(idx, NULL,
				UNPADDED_SIZE_MIN, 1), LZMA_OK);

	assert_uint(lzma_index_memused(idx), <, UINT64_MAX);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_append(void)
{
	// Basic input-ouput test done here.
	// Less trivial tests for this function are done throughout
	// other tests.

	// First test with NULL index
	assert_lzma_ret(lzma_index_append(NULL, NULL, UNPADDED_SIZE_MIN,
			1), LZMA_PROG_ERROR);

	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Test with invalid unpadded size
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN - 1, 1), LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MAX + 1, 1), LZMA_PROG_ERROR);

	// Test with invalid uncompressed size
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MAX, LZMA_VLI_MAX + 1),
			LZMA_PROG_ERROR);

	// Test expected successful block appends
	assert_lzma_ret(lzma_index_append(idx, NULL, UNPADDED_SIZE_MIN,
			1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 2,
			2), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 3,
			3), LZMA_OK);

	lzma_index_end(idx, NULL);

	// Test uncompressed size growing too large.
	// Should result in LZMA_DATA_ERROR
	idx = lzma_index_init(NULL);

	assert_lzma_ret(lzma_index_append(idx, NULL, UNPADDED_SIZE_MAX,
			1), LZMA_DATA_ERROR);

	// Test compressed size growing too large.
	// Should result in LZMA_DATA_ERROR
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, LZMA_VLI_MAX), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_DATA_ERROR);

	// Currently not testing for error case when the size of the index
	// grows too large to be stored. This was not practical to test for
	// since too many blocks needed to be created to cause this.

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_stream_flags(void)
{
	// Only trivial tests done here testing for basic functionality.
	// More in-depth testing for this function will be done in
	// test_lzma_index_checks

	// Testing for NULL inputs
	assert_lzma_ret(lzma_index_stream_flags(NULL, NULL),
			LZMA_PROG_ERROR);

	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	assert_lzma_ret(lzma_index_stream_flags(idx, NULL),
			LZMA_PROG_ERROR);

	lzma_stream_flags stream_flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_CRC32
	};

	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_checks(void)
{
	// Tests should still pass, even if some of the checks
	// are disabled.
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	lzma_stream_flags stream_flags = {
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_NONE
	};

	// First set the check to be none
	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_NONE);

	// Set flags to be crc32 and repeat
	stream_flags.check = LZMA_CHECK_CRC32;
	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_CRC32);

	// Set flags to be crc64 and repeat
	stream_flags.check = LZMA_CHECK_CRC64;
	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_CRC64);

	// Set flags to be SHA256 and repeat
	stream_flags.check = LZMA_CHECK_SHA256;
	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_SHA256);

	// Create second index and cat to first
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	// Set the check to CRC32 for the second index
	stream_flags.check = LZMA_CHECK_CRC32;
	assert_lzma_ret(lzma_index_stream_flags(second, &stream_flags),
			LZMA_OK);

	assert_uint_eq(lzma_index_checks(second),
			LZMA_INDEX_CHECK_MASK_CRC32);

	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);

	// Index should now have both CRC32 and SHA256
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_CRC32 |
			LZMA_INDEX_CHECK_MASK_SHA256);

	// Change stream flags of second stream to be SHA256
	stream_flags.check = LZMA_CHECK_SHA256;
	assert_lzma_ret(lzma_index_stream_flags(idx, &stream_flags),
			LZMA_OK);

	// Index should now have only SHA256
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_SHA256);

	// Test with a third stream
	lzma_index *third = lzma_index_init(NULL);
	assert_true(third != NULL);

	stream_flags.check = LZMA_CHECK_CRC64;
	assert_lzma_ret(lzma_index_stream_flags(third, &stream_flags),
			LZMA_OK);

	assert_uint_eq(lzma_index_checks(third),
			LZMA_INDEX_CHECK_MASK_CRC64);

	assert_lzma_ret(lzma_index_cat(idx, third, NULL), LZMA_OK);

	// Index should now have CRC64 and SHA256
	assert_uint_eq(lzma_index_checks(idx),
			LZMA_INDEX_CHECK_MASK_CRC64 |
			LZMA_INDEX_CHECK_MASK_SHA256);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_stream_padding(void)
{
	// Test NULL index
	assert_lzma_ret(lzma_index_stream_padding(NULL, 0),
			LZMA_PROG_ERROR);

	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Test stream padding not a multiple of 4
	assert_lzma_ret(lzma_index_stream_padding(idx, 3),
			LZMA_PROG_ERROR);

	// Test stream padding too large
	assert_lzma_ret(lzma_index_stream_padding(idx, LZMA_VLI_MAX - 3),
			LZMA_DATA_ERROR);

	// Test stream padding valid
	assert_lzma_ret(lzma_index_stream_padding(idx, 0x1000),
			LZMA_OK);
	assert_lzma_ret(lzma_index_stream_padding(idx, 4),
			LZMA_OK);
	assert_lzma_ret(lzma_index_stream_padding(idx, 0),
			LZMA_OK);

	// Test stream padding causing the file size to grow too large
	assert_lzma_ret(lzma_index_append(idx, NULL,
			LZMA_VLI_MAX - 0x1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_stream_padding(idx, 0x1000),
			LZMA_DATA_ERROR);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_stream_count(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	assert_uint_eq(lzma_index_stream_count(idx), 1);

	// Appending blocks should not change the stream count value
	assert_lzma_ret(lzma_index_append(idx, NULL, UNPADDED_SIZE_MIN,
			1), LZMA_OK);

	assert_uint_eq(lzma_index_stream_count(idx), 1);

	// Test with multiple streams
	for (uint32_t i = 0; i < 100; i++) {
		lzma_index *idx_cat = lzma_index_init(NULL);
		assert_true(idx != NULL);
		assert_lzma_ret(lzma_index_cat(idx, idx_cat, NULL), LZMA_OK);
		assert_uint_eq(lzma_index_stream_count(idx), i + 2);
	}

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_block_count(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	assert_uint_eq(lzma_index_block_count(idx), 0);

	const uint32_t iterations = 0x1000;
	for (uint32_t i = 0; i < iterations; i++) {
		assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);
		assert_uint_eq(lzma_index_block_count(idx), i + 1);
	}

	// Create new index with a few blocks
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	assert_lzma_ret(lzma_index_append(second, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(second, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(second, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	assert_uint_eq(lzma_index_block_count(second), 3);

	// cat the streams together and the result should have
	// the sum of the two individual counts
	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);
	assert_uint_eq(lzma_index_block_count(idx), iterations + 3);

	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);

	assert_uint_eq(lzma_index_block_count(idx), iterations + 4);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_size(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Base size should be:
	// 1 byte index indicator
	// 1 byte number of records
	// 0 bytes records
	// 2 bytes index padding
	// 4 bytes CRC32
	// Total: 8 bytes
	assert_uint_eq(lzma_index_size(idx), 8);

	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);
	// New size should be:
	// 1 byte index indicator
	// 1 byte number of records
	// 2 bytes records
	// 0 bytes index padding
	// 4 bytes CRC32
	// Total: 8 bytes
	assert_uint_eq(lzma_index_size(idx), 8);

	assert_lzma_ret(lzma_index_append(idx, NULL,
			LZMA_VLI_MAX / 4, LZMA_VLI_MAX / 4), LZMA_OK);
	// New size should be:
	// 1 byte index indicator
	// 1 byte number of records
	// 20 bytes records
	// 2 bytes index padding
	// 4 bytes CRC32
	// Total: 28 bytes
	assert_uint_eq(lzma_index_size(idx), 28);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_stream_size(void)
{
	// Index stream size calculated by:
	// Size of header (12 bytes)
	// Size of all blocks
	// Size of the index
	// Size of the stream footer
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// First test with empty index
	// Size should be:
	// Size of header - 12 bytes
	// Size of all blocks - 0 bytes
	// Size of index - 8 bytes
	// Size of stream footer - 12 bytes
	// Total: 32 bytes
	assert_uint_eq(lzma_index_stream_size(idx), 32);
	// Next, append a few blocks and retest
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	// Size should be:
	// Size of header - 12 bytes
	// Size of all blocks - 3000 bytes
	// Size of idx - 16 bytes
	// Size of stream footer - 12 bytes
	// Total: 3040 bytes
	assert_uint_eq(lzma_index_stream_size(idx), 3040);

	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	assert_uint_eq(lzma_index_stream_size(second), 32);
	assert_lzma_ret(lzma_index_append(second, NULL, 1000, 1), LZMA_OK);
	// Size should be:
	// Size of header - 12 bytes
	// Size of all blocks - 1000 bytes
	// Size of idx - 12 bytes
	// Size of stream footer - 12 bytes
	// Total: 1036 bytes
	assert_uint_eq(lzma_index_stream_size(second), 1036);

	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);
	// Size should be:
	// Size of header - 12 bytes
	// Size of all blocks - 4000 bytes
	// Size of idx - 20 bytes
	// Size of stream footer - 12 bytes
	// Total: 4044 bytes
	assert_uint_eq(lzma_index_stream_size(idx), 4044);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_total_size(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// First test empty index.
	// Result should be 0 since no blocks have been added
	assert_uint_eq(lzma_index_total_size(idx), 0);

	// Add a few blocks and retest after each append
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_uint_eq(lzma_index_total_size(idx), 1000);

	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_uint_eq(lzma_index_total_size(idx), 2000);

	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_uint_eq(lzma_index_total_size(idx), 3000);

	// Create second index and append blocks to it.
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	assert_uint_eq(lzma_index_total_size(second), 0);

	assert_lzma_ret(lzma_index_append(second, NULL, 100, 1), LZMA_OK);
	assert_uint_eq(lzma_index_total_size(second), 100);

	assert_lzma_ret(lzma_index_append(second, NULL, 100, 1), LZMA_OK);
	assert_uint_eq(lzma_index_total_size(second), 200);

	// cat the streams together
	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);
	// The result total size should be size of index blocks +
	// size of second blocks.
	assert_uint_eq(lzma_index_total_size(idx), 3200);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_file_size(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Should be the same as test_lzma_index_stream_size with
	// only one stream and no stream padding.
	assert_uint_eq(lzma_index_file_size(idx), 32);

	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);

	assert_uint_eq(lzma_index_file_size(idx), 3040);

	// Next add stream padding
	assert_lzma_ret(lzma_index_stream_padding(idx, 1000),
			LZMA_OK);

	assert_uint_eq(lzma_index_file_size(idx), 4040);

	// Create second index.
	// Very similar to test_lzma_index_stream_size, but
	// the values should include the headers of the second stream
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	assert_lzma_ret(lzma_index_append(second, NULL, 1000, 1), LZMA_OK);
	assert_uint_eq(lzma_index_stream_size(second), 1036);

	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);

	// Size should be:
	// Size of header - 12 * 2 bytes
	// Size of all blocks - 3000 + 1000 bytes
	// Size of index - 16 + 12 bytes
	// Size of stream padding - 1000 bytes
	// Size of stream footer - 12 * 2 bytes
	// Total: 5076 bytes
	assert_uint_eq(lzma_index_file_size(idx), 5076);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_uncompressed_size(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Empty index should have 0 uncompressed size
	assert_uint_eq(lzma_index_uncompressed_size(idx), 0);

	// Append a few small blocks
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 10), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 100), LZMA_OK);

	assert_uint_eq(lzma_index_uncompressed_size(idx), 111);

	// Create another index
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	// Append a few small blocks
	assert_lzma_ret(lzma_index_append(second, NULL, 1000, 2), LZMA_OK);
	assert_lzma_ret(lzma_index_append(second, NULL, 1000, 20), LZMA_OK);
	assert_lzma_ret(lzma_index_append(second, NULL, 1000, 200), LZMA_OK);

	assert_uint_eq(lzma_index_uncompressed_size(second), 222);

	// Cat second index to first
	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);

	// New uncompressed size should be sum of index and second
	assert_uint_eq(lzma_index_uncompressed_size(idx), 333);

	// Append one more block to index and ensure it is properly updated
	assert_lzma_ret(lzma_index_append(idx, NULL, 1000, 111), LZMA_OK);
	assert_uint_eq(lzma_index_uncompressed_size(idx), 444);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_iter_init(void)
{
	// Testing basic init functionality.
	// The init function should call rewind on the iterator
	lzma_index *first = lzma_index_init(NULL);
	assert_true(first != NULL);

	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	lzma_index *third = lzma_index_init(NULL);
	assert_true(third != NULL);

	assert_lzma_ret(lzma_index_cat(first, second, NULL), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(first, third, NULL), LZMA_OK);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, first);

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	assert_uint_eq(iter.stream.number, 1);
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	assert_uint_eq(iter.stream.number, 2);

	lzma_index_iter_init(&iter, first);

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	assert_uint_eq(iter.stream.number, 3);
}


static void
test_lzma_index_iter_rewind(void)
{
	lzma_index *first = lzma_index_init(NULL);
	assert_true(first != NULL);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, first);

	// Append 3 blocks and iterate to them
	for (int i = 0; i < 3; i++) {
		assert_lzma_ret(lzma_index_append(first, NULL,
				UNPADDED_SIZE_MIN, 1), LZMA_OK);
		assert_false(lzma_index_iter_next(&iter,
				LZMA_INDEX_ITER_BLOCK));
		assert_uint_eq(iter.block.number_in_file, i + 1);
	}

	// Rewind back to the begining and iterate over the blocks again
	lzma_index_iter_rewind(&iter);

	for (int i = 0; i < 3; i++) {
		assert_false(lzma_index_iter_next(&iter,
				LZMA_INDEX_ITER_BLOCK));
		assert_uint_eq(iter.block.number_in_file, i + 1);
	}

	// Next cat two more indexes, iterate over them,
	// rewind, and iterate over them again.
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	lzma_index *third = lzma_index_init(NULL);
	assert_true(third != NULL);

	assert_lzma_ret(lzma_index_cat(first, second, NULL), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(first, third, NULL), LZMA_OK);

	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_STREAM));
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_STREAM));

	assert_uint_eq(iter.stream.number, 3);

	lzma_index_iter_rewind(&iter);

	for (int i = 0; i < 3; i++) {
		assert_false(lzma_index_iter_next(&iter,
				LZMA_INDEX_ITER_STREAM));
		assert_uint_eq(iter.stream.number, i + 1);
	}

	lzma_index_end(first, NULL);
}


static void
test_lzma_index_iter_next(void)
{
	lzma_index *first = lzma_index_init(NULL);
	assert_true(first != NULL);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, first);

	// First test bad mode values
	for (int i = LZMA_INDEX_ITER_NONEMPTY_BLOCK + 1; i < 100; i++)
		assert_true(lzma_index_iter_next(&iter, i));

	// Test iterating over blocks
	assert_lzma_ret(lzma_index_append(first, NULL,
			UNPADDED_SIZE_MIN, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(first, NULL,
			UNPADDED_SIZE_MIN * 2, 10), LZMA_OK);
	assert_lzma_ret(lzma_index_append(first, NULL,
			UNPADDED_SIZE_MIN * 3, 100), LZMA_OK);

	// For blocks, need to verify:
	// - number_in_file (overall block number)
	// - compressed_file_offset
	// - uncompressed_file_offset
	// - number_in_stream (block number relative to current stream)
	// - compressed_stream_offset
	// - uncompressed_stream_offset
	// - uncompressed_size
	// - unpadded_size
	// - total_size

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	// Verify block data stored correctly
	assert_uint_eq(iter.block.number_in_file, 1);
	// Should start right after the header
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);
	assert_uint_eq(iter.block.number_in_stream, 1);
	assert_uint_eq(iter.block.compressed_stream_offset,
			LZMA_STREAM_HEADER_SIZE);
	assert_uint_eq(iter.block.uncompressed_stream_offset, 0);
	assert_uint_eq(iter.block.unpadded_size, UNPADDED_SIZE_MIN);
	assert_uint_eq(iter.block.total_size, vli_ceil4(UNPADDED_SIZE_MIN));

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	// Verify block data stored correctly
	assert_uint_eq(iter.block.number_in_file, 2);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE +
			vli_ceil4(UNPADDED_SIZE_MIN));
	assert_uint_eq(iter.block.uncompressed_file_offset, 1);
	assert_uint_eq(iter.block.number_in_stream, 2);
	assert_uint_eq(iter.block.compressed_stream_offset,
			LZMA_STREAM_HEADER_SIZE +
			vli_ceil4(UNPADDED_SIZE_MIN));
	assert_uint_eq(iter.block.uncompressed_stream_offset, 1);
	assert_uint_eq(iter.block.unpadded_size, UNPADDED_SIZE_MIN * 2);
	assert_uint_eq(iter.block.total_size, vli_ceil4(UNPADDED_SIZE_MIN * 2));

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	// Verify block data stored correctly
	assert_uint_eq(iter.block.number_in_file, 3);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE +
			vli_ceil4(UNPADDED_SIZE_MIN) +
			vli_ceil4(UNPADDED_SIZE_MIN * 2));
	assert_uint_eq(iter.block.uncompressed_file_offset, 11);
	assert_uint_eq(iter.block.number_in_stream, 3);
	assert_uint_eq(iter.block.compressed_stream_offset,
			LZMA_STREAM_HEADER_SIZE +
			vli_ceil4(UNPADDED_SIZE_MIN) +
			vli_ceil4(UNPADDED_SIZE_MIN * 2));
	assert_uint_eq(iter.block.uncompressed_stream_offset, 11);
	assert_uint_eq(iter.block.unpadded_size, UNPADDED_SIZE_MIN * 3);
	assert_uint_eq(iter.block.total_size,
			vli_ceil4(UNPADDED_SIZE_MIN * 3));

	// Only three blocks were added, so this should return true
	assert_true(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	const uint32_t second_stream_compressed_start =
			LZMA_STREAM_HEADER_SIZE * 2 +
			vli_ceil4(UNPADDED_SIZE_MIN) +
			vli_ceil4(UNPADDED_SIZE_MIN * 2) +
			vli_ceil4(UNPADDED_SIZE_MIN * 3) +
			lzma_index_size(first);
	const uint32_t second_stream_uncompressed_start = 1 + 10 + 100;

	// Test iterating over streams.
	// The second stream will have 0 blocks
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	// Set stream flags for stream 2
	lzma_stream_flags flags = {
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_CRC32
	};
	assert_lzma_ret(lzma_index_stream_flags(second, &flags), LZMA_OK);

	// The second stream will have 8 bytes padding
	assert_lzma_ret(lzma_index_stream_padding(second, 8), LZMA_OK);

	const uint32_t second_stream_index_size = lzma_index_size(second);

	// The third stream will have 2 blocks
	lzma_index *third = lzma_index_init(NULL);
	assert_true(third != NULL);

	assert_lzma_ret(lzma_index_append(third, NULL, 32, 20), LZMA_OK);
	assert_lzma_ret(lzma_index_append(third, NULL, 64, 40), LZMA_OK);

	const uint32_t third_stream_index_size = lzma_index_size(third);

	assert_lzma_ret(lzma_index_cat(first, second, NULL), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(first, third, NULL), LZMA_OK);

	// For streams, need to verify:
	// - flags (stream flags)
	// - number (stream count)
	// - block_count
	// - compressed_offset
	// - uncompressed_offset
	// - compressed_size
	// - uncompressed_size
	// - padding (stream padding)
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));

	// Verify stream
	assert_uint_eq(iter.stream.flags->backward_size,
			LZMA_BACKWARD_SIZE_MIN);
	assert_uint_eq(iter.stream.flags->check, LZMA_CHECK_CRC32);
	assert_uint_eq(iter.stream.number, 2);
	assert_uint_eq(iter.stream.block_count, 0);
	assert_uint_eq(iter.stream.compressed_offset,
			second_stream_compressed_start);
	assert_uint_eq(iter.stream.uncompressed_offset,
			second_stream_uncompressed_start);
	assert_uint_eq(iter.stream.compressed_size,
			LZMA_STREAM_HEADER_SIZE * 2 +
			second_stream_index_size);
	assert_uint_eq(iter.stream.uncompressed_size, 0);
	assert_uint_eq(iter.stream.padding, 8);

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));

	// Verify stream
	const uint32_t third_stream_compressed_start =
			second_stream_compressed_start +
			LZMA_STREAM_HEADER_SIZE * 2 +
			8 + // Stream padding
			second_stream_index_size;
	const uint32_t third_stream_uncompressed_start =
			second_stream_uncompressed_start;

	assert_uint_eq(iter.stream.number, 3);
	assert_uint_eq(iter.stream.block_count, 2);
	assert_uint_eq(iter.stream.compressed_offset,
			third_stream_compressed_start);
	assert_uint_eq(iter.stream.uncompressed_offset,
			third_stream_uncompressed_start);
	assert_uint_eq(iter.stream.compressed_size,
			LZMA_STREAM_HEADER_SIZE * 2 +
			96 + // Total compressed size
			third_stream_index_size);
	assert_uint_eq(iter.stream.uncompressed_size, 60);
	assert_uint_eq(iter.stream.padding, 0);

	assert_true(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM));
	// Even after a failing call to next with ITER_STREAM mode,
	// should still be able to iterate over the 2 blocks in
	// stream 3
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	// Verify both blocks

	// Next call to iterate block should return true because the
	// first block can already be read from the LZMA_INDEX_ITER_STREAM
	// call
	assert_true(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));

	// Rewind to test LZMA_INDEX_ITER_ANY
	lzma_index_iter_rewind(&iter);

	// Iterate past the first three blocks
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));

	// Iterate past the next stream
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));

	// Iterate past the next stream
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));
	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));

	// Last call should fail
	assert_true(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));

	// Rewind to test LZMA_INDEX_ITER_NONEMPTY_BLOCK
	lzma_index_iter_rewind(&iter);

	// Iterate past the first three blocks
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_NONEMPTY_BLOCK));
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_NONEMPTY_BLOCK));
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_NONEMPTY_BLOCK));

	// Skip past the next stream
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_NONEMPTY_BLOCK));
	// Iterate past the next stream
	assert_false(lzma_index_iter_next(&iter,
			LZMA_INDEX_ITER_NONEMPTY_BLOCK));

	// Last call should fail
	assert_true(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY));

	lzma_index_end(first, NULL);
}


static void
test_lzma_index_iter_locate(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, idx);

	// Cannot locate anything from an empty Index.
	assert_true(lzma_index_iter_locate(&iter, 0));
	assert_true(lzma_index_iter_locate(&iter, 555));

	// One empty Record: nothing is found since there's no uncompressed
	// data.
	assert_lzma_ret(lzma_index_append(idx, NULL, 16, 0), LZMA_OK);
	assert_true(lzma_index_iter_locate(&iter, 0));

	// Non-empty Record and we can find something.
	assert_lzma_ret(lzma_index_append(idx, NULL, 32, 5), LZMA_OK);
	assert_false(lzma_index_iter_locate(&iter, 0));
	assert_uint_eq(iter.block.total_size, 32);
	assert_uint_eq(iter.block.uncompressed_size, 5);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	// Still cannot find anything past the end.
	assert_true(lzma_index_iter_locate(&iter, 5));

	// Add the third Record.
	assert_lzma_ret(lzma_index_append(idx, NULL, 40, 11), LZMA_OK);

	assert_false(lzma_index_iter_locate(&iter, 0));
	assert_uint_eq(iter.block.total_size, 32);
	assert_uint_eq(iter.block.uncompressed_size, 5);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	assert_false(lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK));
	assert_uint_eq(iter.block.total_size, 40);
	assert_uint_eq(iter.block.uncompressed_size, 11);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16 + 32);
	assert_uint_eq(iter.block.uncompressed_file_offset, 5);

	assert_false(lzma_index_iter_locate(&iter, 2));
	assert_uint_eq(iter.block.total_size, 32);
	assert_uint_eq(iter.block.uncompressed_size, 5);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	assert_false(lzma_index_iter_locate(&iter, 5));
	assert_uint_eq(iter.block.total_size, 40);
	assert_uint_eq(iter.block.uncompressed_size, 11);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16 + 32);
	assert_uint_eq(iter.block.uncompressed_file_offset, 5);

	assert_false(lzma_index_iter_locate(&iter, 5 + 11 - 1));
	assert_uint_eq(iter.block.total_size, 40);
	assert_uint_eq(iter.block.uncompressed_size, 11);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 16 + 32);
	assert_uint_eq(iter.block.uncompressed_file_offset, 5);

	assert_true(lzma_index_iter_locate(&iter, 5 + 11));
	assert_true(lzma_index_iter_locate(&iter, 5 + 15));

	// Large Index
	lzma_index_end(idx, NULL);
	idx = lzma_index_init(NULL);
	assert_true(idx != NULL);
	lzma_index_iter_init(&iter, idx);

	for (size_t n = 4; n <= 4 * 5555; n += 4)
		assert_lzma_ret(lzma_index_append(idx, NULL, n + 8, n),
				LZMA_OK);

	assert_uint_eq(lzma_index_block_count(idx), 5555);

	// First Record
	assert_false(lzma_index_iter_locate(&iter, 0));
	assert_uint_eq(iter.block.total_size, 4 + 8);
	assert_uint_eq(iter.block.uncompressed_size, 4);
	assert_uint_eq(iter.block.compressed_file_offset, LZMA_STREAM_HEADER_SIZE);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	assert_false(lzma_index_iter_locate(&iter, 3));
	assert_uint_eq(iter.block.total_size, 4 + 8);
	assert_uint_eq(iter.block.uncompressed_size, 4);
	assert_uint_eq(iter.block.compressed_file_offset, LZMA_STREAM_HEADER_SIZE);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	// Second Record
	assert_false(lzma_index_iter_locate(&iter, 4));
	assert_uint_eq(iter.block.total_size, 2 * 4 + 8);
	assert_uint_eq(iter.block.uncompressed_size, 2 * 4);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + 4 + 8);
	assert_uint_eq(iter.block.uncompressed_file_offset, 4);

	// Last Record
	assert_false(lzma_index_iter_locate(
			&iter, lzma_index_uncompressed_size(idx) - 1));
	assert_uint_eq(iter.block.total_size, 4 * 5555 + 8);
	assert_uint_eq(iter.block.uncompressed_size, 4 * 5555);
	assert_uint_eq(iter.block.compressed_file_offset,
			lzma_index_total_size(idx)
			+ LZMA_STREAM_HEADER_SIZE - 4 * 5555 - 8);
	assert_uint_eq(iter.block.uncompressed_file_offset,
			lzma_index_uncompressed_size(idx) - 4 * 5555);

	// Allocation chunk boundaries. See INDEX_GROUP_SIZE in
	// liblzma/common/index.c.
	const size_t group_multiple = 256 * 4;
	const size_t radius = 8;
	const size_t start = group_multiple - radius;
	lzma_vli ubase = 0;
	lzma_vli tbase = 0;
	size_t n;
	for (n = 1; n < start; ++n) {
		ubase += n * 4;
		tbase += n * 4 + 8;
	}

	while (n < start + 2 * radius) {
		assert_false(lzma_index_iter_locate(&iter, ubase + n * 4));

		assert_uint_eq(iter.block.compressed_file_offset,
				tbase + n * 4 + 8
				+ LZMA_STREAM_HEADER_SIZE);
		assert_uint_eq(iter.block.uncompressed_file_offset,
				ubase + n * 4);

		tbase += n * 4 + 8;
		ubase += n * 4;
		++n;

		assert_uint_eq(iter.block.total_size, n * 4 + 8);
		assert_uint_eq(iter.block.uncompressed_size, n * 4);
	}

	// Do it also backwards.
	while (n > start) {
		assert_false(lzma_index_iter_locate(&iter, ubase + (n - 1) * 4));

		assert_uint_eq(iter.block.total_size, n * 4 + 8);
		assert_uint_eq(iter.block.uncompressed_size, n * 4);

		--n;
		tbase -= n * 4 + 8;
		ubase -= n * 4;

		assert_uint_eq(iter.block.compressed_file_offset,
				tbase + n * 4 + 8
				+ LZMA_STREAM_HEADER_SIZE);
		assert_uint_eq(iter.block.uncompressed_file_offset,
				ubase + n * 4);
	}

	// Test locating in concatenated Index.
	lzma_index_end(idx, NULL);
	idx = lzma_index_init(NULL);
	assert_true(idx != NULL);
	lzma_index_iter_init(&iter, idx);
	for (n = 0; n < group_multiple; ++n)
		assert_lzma_ret(lzma_index_append(idx, NULL, 8, 0),
				LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL, 16, 1), LZMA_OK);
	assert_false(lzma_index_iter_locate(&iter, 0));
	assert_uint_eq(iter.block.total_size, 16);
	assert_uint_eq(iter.block.uncompressed_size, 1);
	assert_uint_eq(iter.block.compressed_file_offset,
			LZMA_STREAM_HEADER_SIZE + group_multiple * 8);
	assert_uint_eq(iter.block.uncompressed_file_offset, 0);

	lzma_index_end(idx, NULL);
}


static void
test_lzma_index_cat(void)
{
	// Most complex tests for this function are done in other tests.
	// This will mostly test basic functionality.

	lzma_index *dest = lzma_index_init(NULL);
	assert_true(dest != NULL);

	lzma_index *src = lzma_index_init(NULL);
	assert_true(src != NULL);

	// First test NULL dest or src
	assert_lzma_ret(lzma_index_cat(NULL, NULL, NULL), LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_cat(dest, NULL, NULL), LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_cat(NULL, src, NULL), LZMA_PROG_ERROR);

	// Check for uncompressed size overflow
	assert_lzma_ret(lzma_index_append(dest, NULL,
			(UNPADDED_SIZE_MAX / 2) + 1, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(src, NULL,
			(UNPADDED_SIZE_MAX / 2) + 1, 1), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(dest, src, NULL), LZMA_DATA_ERROR);

	// Check for compressed size overflow
	dest = lzma_index_init(NULL);
	assert_true(dest != NULL);

	src = lzma_index_init(NULL);
	assert_true(src != NULL);

	assert_lzma_ret(lzma_index_append(dest, NULL,
			UNPADDED_SIZE_MIN, LZMA_VLI_MAX - 1), LZMA_OK);
	assert_lzma_ret(lzma_index_append(src, NULL,
			UNPADDED_SIZE_MIN, LZMA_VLI_MAX - 1), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(dest, src, NULL), LZMA_DATA_ERROR);

	lzma_index_end(dest, NULL);
	lzma_index_end(src, NULL);
}


// Helper function for test_lzma_index_dup
static bool
index_is_equal(const lzma_index *a, const lzma_index *b)
{
	// Compare only the Stream and Block sizes and offsets.
	lzma_index_iter ra, rb;
	lzma_index_iter_init(&ra, a);
	lzma_index_iter_init(&rb, b);

	while (true) {
		bool reta = lzma_index_iter_next(&ra, LZMA_INDEX_ITER_ANY);
		bool retb = lzma_index_iter_next(&rb, LZMA_INDEX_ITER_ANY);
		if (reta)
			return !(reta ^ retb);

		if (ra.stream.number != rb.stream.number
				|| ra.stream.block_count
					!= rb.stream.block_count
				|| ra.stream.compressed_offset
					!= rb.stream.compressed_offset
				|| ra.stream.uncompressed_offset
					!= rb.stream.uncompressed_offset
				|| ra.stream.compressed_size
					!= rb.stream.compressed_size
				|| ra.stream.uncompressed_size
					!= rb.stream.uncompressed_size
				|| ra.stream.padding
					!= rb.stream.padding)
			return false;

		if (ra.stream.block_count == 0)
			continue;

		if (ra.block.number_in_file != rb.block.number_in_file
				|| ra.block.compressed_file_offset
					!= rb.block.compressed_file_offset
				|| ra.block.uncompressed_file_offset
					!= rb.block.uncompressed_file_offset
				|| ra.block.number_in_stream
					!= rb.block.number_in_stream
				|| ra.block.compressed_stream_offset
					!= rb.block.compressed_stream_offset
				|| ra.block.uncompressed_stream_offset
					!= rb.block.uncompressed_stream_offset
				|| ra.block.uncompressed_size
					!= rb.block.uncompressed_size
				|| ra.block.unpadded_size
					!= rb.block.unpadded_size
				|| ra.block.total_size
					!= rb.block.total_size)
			return false;
	}
}


// Allocator that succeeds for the first two allocation but fails the rest.
static void *
my_alloc(void *opaque, size_t a, size_t b)
{
	(void)opaque;

	static unsigned count = 0;
	if (++count > 2)
		return NULL;

	return malloc(a * b);
}

static const lzma_allocator test_index_dup_alloc = { &my_alloc, NULL, NULL };


static void
test_lzma_index_dup(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	// Test for the bug fix 21515d79d778b8730a434f151b07202d52a04611:
	// liblzma: Fix lzma_index_dup() for empty Streams.
	assert_lzma_ret(lzma_index_stream_padding(idx, 4), LZMA_OK);
	lzma_index *copy = lzma_index_dup(idx, NULL);
	assert_true(copy != NULL);
	assert_true(index_is_equal(idx, copy));
	lzma_index_end(copy, NULL);

	// Test for the bug fix 3bf857edfef51374f6f3fffae3d817f57d3264a0:
	// liblzma: Fix a memory leak in error path of lzma_index_dup().
	// Use Valgrind to see that there are no leaks.
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 10), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 2, 100), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 3, 1000), LZMA_OK);

	assert_true(lzma_index_dup(idx, &test_index_dup_alloc) == NULL);

	// Test a few streams and blocks
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	assert_lzma_ret(lzma_index_stream_padding(second, 16), LZMA_OK);

	lzma_index *third = lzma_index_init(NULL);
	assert_true(third != NULL);

	assert_lzma_ret(lzma_index_append(third, NULL,
			UNPADDED_SIZE_MIN * 10, 40), LZMA_OK);
	assert_lzma_ret(lzma_index_append(third, NULL,
			UNPADDED_SIZE_MIN * 20, 400), LZMA_OK);
	assert_lzma_ret(lzma_index_append(third, NULL,
			UNPADDED_SIZE_MIN * 30, 4000), LZMA_OK);

	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);
	assert_lzma_ret(lzma_index_cat(idx, third, NULL), LZMA_OK);

	copy = lzma_index_dup(idx, NULL);
	assert_true(copy != NULL);
	assert_true(index_is_equal(idx, copy));

	lzma_index_end(idx, NULL);
}

#if defined(HAVE_ENCODERS) && defined(HAVE_DECODERS)
static void
verify_index_buffer(const lzma_index *idx, const uint8_t *buffer,
		const uint32_t buffer_size)
{
	lzma_index_iter iter;
	lzma_index_iter_init(&iter, idx);

	size_t buffer_pos = 0;
	// Verify index indicator
	assert_uint_eq(buffer[buffer_pos++], 0);

	// Get number of records
	lzma_vli number_of_records = 0;
	lzma_vli block_count = 0;
	assert_lzma_ret(lzma_vli_decode(&number_of_records, NULL, buffer,
			&buffer_pos, buffer_size), LZMA_OK);

	while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY)) {
		// Verify each record
		// Verify unpadded size
		lzma_vli unpadded_size, uncompressed_size;
		assert_lzma_ret(lzma_vli_decode(&unpadded_size,
				NULL, buffer, &buffer_pos,
				buffer_size), LZMA_OK);
		assert_uint_eq(unpadded_size,
				iter.block.unpadded_size);

		// Verify uncompressed size
		assert_lzma_ret(lzma_vli_decode(&uncompressed_size,
				NULL, buffer, &buffer_pos,
				buffer_size), LZMA_OK);
		assert_uint_eq(uncompressed_size,
				iter.block.uncompressed_size);

		block_count++;
	}

	// Verify number of records
	assert_uint_eq(number_of_records, block_count);

	// Verify index padding
	for (; buffer_pos % 4 != 0; buffer_pos++)
		assert_uint_eq(buffer[buffer_pos], 0);

	// Verify crc32
	uint32_t crc32 = lzma_crc32(buffer, buffer_pos, 0);
	assert_uint_eq(read32le(buffer + buffer_pos), crc32);
}
#endif


static void
test_lzma_index_encoder(void)
{
#if !defined(HAVE_ENCODERS) || !defined(HAVE_DECODERS)
	assert_skip("Encoder or decoder support disabled");
#else
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	lzma_stream strm = LZMA_STREAM_INIT;

	// First do basic NULL checks
	assert_lzma_ret(lzma_index_encoder(NULL, NULL), LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_encoder(&strm, NULL), LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_encoder(NULL, idx), LZMA_PROG_ERROR);

	// Append three small blocks
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 10), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 2, 100), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 3, 1000), LZMA_OK);

	// Encode this index into a buffer
	uint32_t buffer_size = lzma_index_size(idx);
	uint8_t *buffer = tuktest_malloc(buffer_size);

	assert_lzma_ret(lzma_index_encoder(&strm, idx), LZMA_OK);

	strm.avail_out = buffer_size;
	strm.next_out = buffer;

	assert_lzma_ret(lzma_code(&strm, LZMA_FINISH), LZMA_STREAM_END);
	assert_uint_eq(strm.avail_out, 0);

	lzma_end(&strm);

	verify_index_buffer(idx, buffer, buffer_size);

	// Test with multiple streams concatenated into 1 index
	lzma_index *second = lzma_index_init(NULL);
	assert_true(second != NULL);

	// Include 1 block
	assert_lzma_ret(lzma_index_append(second, NULL,
			UNPADDED_SIZE_MIN * 4, 20), LZMA_OK);

	// Include stream padding
	assert_lzma_ret(lzma_index_stream_padding(second, 16), LZMA_OK);

	assert_lzma_ret(lzma_index_cat(idx, second, NULL), LZMA_OK);
	buffer_size = lzma_index_size(idx);
	buffer = tuktest_malloc(buffer_size);
	assert_lzma_ret(lzma_index_encoder(&strm, idx), LZMA_OK);

	strm.avail_out = buffer_size;
	strm.next_out = buffer;

	assert_lzma_ret(lzma_code(&strm, LZMA_FINISH), LZMA_STREAM_END);
	assert_uint_eq(strm.avail_out, 0);

	verify_index_buffer(idx, buffer, buffer_size);

	lzma_end(&strm);
#endif
}

static void
generate_index_decode_buffer(void)
{
#ifdef HAVE_ENCODERS
	decode_test_index = lzma_index_init(NULL);
	if (decode_test_index == NULL)
		return;

	// Add 4 blocks
	for (int i = 1; i < 5; i++)
		if (lzma_index_append(decode_test_index, NULL,
				0x1000 * i, 0x100 * i) != LZMA_OK)
			return;

	size_t size = lzma_index_size(decode_test_index);
	decode_buffer = tuktest_malloc(size);

	if (lzma_index_buffer_encode(decode_test_index,
			decode_buffer, &decode_buffer_size, size) != LZMA_OK)
		decode_buffer_size = 0;
#endif
}


#ifdef HAVE_DECODERS
static void
decode_index(const uint8_t *buffer, const size_t size, lzma_stream *strm,
		lzma_ret expected_error)
{
	strm->avail_in = size;
	strm->next_in = buffer;
	assert_lzma_ret(lzma_code(strm, LZMA_FINISH), expected_error);
}
#endif


static void
test_lzma_index_decoder(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support disabled");
#else
	if (decode_buffer_size == 0)
		assert_skip("Could not initialize decode test buffer");

	lzma_stream strm = LZMA_STREAM_INIT;

	assert_lzma_ret(lzma_index_decoder(NULL, NULL, MEMLIMIT),
			LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_decoder(&strm, NULL, MEMLIMIT),
			LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_decoder(NULL, &decode_test_index,
			MEMLIMIT), LZMA_PROG_ERROR);

	// Do actual decode
	lzma_index *idx;
	assert_lzma_ret(lzma_index_decoder(&strm, &idx, MEMLIMIT),
			LZMA_OK);

	decode_index(decode_buffer, decode_buffer_size, &strm,
			LZMA_STREAM_END);

	// Compare results with expected
	assert_true(index_is_equal(decode_test_index, idx));

	lzma_index_end(idx, NULL);

	// Test again with too low memory limit
	assert_lzma_ret(lzma_index_decoder(&strm, &idx, 0), LZMA_OK);

	decode_index(decode_buffer, decode_buffer_size, &strm,
			LZMA_MEMLIMIT_ERROR);

	uint8_t *corrupt_buffer = tuktest_malloc(decode_buffer_size);
	memcpy(corrupt_buffer, decode_buffer, decode_buffer_size);

	assert_lzma_ret(lzma_index_decoder(&strm, &idx, MEMLIMIT),
			LZMA_OK);

	// First corrupt the index indicator
	corrupt_buffer[0] ^= 1;
	decode_index(corrupt_buffer, decode_buffer_size, &strm,
			LZMA_DATA_ERROR);
	corrupt_buffer[0] ^= 1;

	// Corrupt something in middle of index
	corrupt_buffer[decode_buffer_size / 2] ^= 1;
	assert_lzma_ret(lzma_index_decoder(&strm, &idx, MEMLIMIT),
			LZMA_OK);
	decode_index(corrupt_buffer, decode_buffer_size, &strm,
			LZMA_DATA_ERROR);
	corrupt_buffer[decode_buffer_size / 2] ^= 1;

	// Corrupt CRC32
	corrupt_buffer[decode_buffer_size - 1] ^= 1;
	assert_lzma_ret(lzma_index_decoder(&strm, &idx, MEMLIMIT),
			LZMA_OK);
	decode_index(corrupt_buffer, decode_buffer_size, &strm,
			LZMA_DATA_ERROR);
	corrupt_buffer[decode_buffer_size - 1] ^= 1;

	// Corrupt padding by setting it to non zero
	corrupt_buffer[decode_buffer_size - 5] ^= 1;
	assert_lzma_ret(lzma_index_decoder(&strm, &idx, MEMLIMIT),
			LZMA_OK);
	decode_index(corrupt_buffer, decode_buffer_size, &strm,
			LZMA_DATA_ERROR);
	corrupt_buffer[decode_buffer_size - 1] ^= 1;

	lzma_end(&strm);
#endif
}


static void
test_lzma_index_buffer_encode(void)
{
#if !defined(HAVE_ENCODERS) || !defined(HAVE_DECODERS)
	assert_skip("Encoder or decoder support disabled");
#else
	// More simple test than test_lzma_index_encoder because
	// currently lzma_index_buffer_encode is mostly a wrapper
	// around lzma_index_encoder anyway
	lzma_index *idx = lzma_index_init(NULL);
	assert_true(idx != NULL);

	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN, 10), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 2, 100), LZMA_OK);
	assert_lzma_ret(lzma_index_append(idx, NULL,
			UNPADDED_SIZE_MIN * 3, 1000), LZMA_OK);

	uint32_t buffer_size = lzma_index_size(idx);
	uint8_t *buffer = tuktest_malloc(buffer_size);
	size_t out_pos = 1;

	// First test bad arguments
	assert_lzma_ret(lzma_index_buffer_encode(NULL, NULL, NULL, 0),
			LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_buffer_encode(idx, NULL, NULL, 0),
			LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_buffer_encode(idx, buffer, NULL, 0),
			LZMA_PROG_ERROR);
	assert_lzma_ret(lzma_index_buffer_encode(idx, buffer, &out_pos,
			0), LZMA_PROG_ERROR);
	out_pos = 0;
	assert_lzma_ret(lzma_index_buffer_encode(idx, buffer, &out_pos,
			1), LZMA_BUF_ERROR);

	// Do encoding
	assert_lzma_ret(lzma_index_buffer_encode(idx, buffer, &out_pos,
			buffer_size), LZMA_OK);
	assert_uint_eq(out_pos, buffer_size);

	// Validate results
	verify_index_buffer(idx, buffer, buffer_size);
#endif
}


static void
test_lzma_index_buffer_decode(void)
{
#ifndef HAVE_DECODERS
	assert_skip("Decoder support disabled");
#else
	if (decode_buffer_size == 0)
		assert_skip("Could not initialize decode test buffer");

	// Simple test since test_lzma_index_decoder covers most of the
	// lzma_index_buffer_decode anyway.

	// First test NULL checks
	assert_lzma_ret(lzma_index_buffer_decode(NULL, NULL, NULL, NULL,
			NULL, 0), LZMA_PROG_ERROR);

	lzma_index *idx;
	uint64_t memlimit = MEMLIMIT;
	size_t in_pos = 0;

	assert_lzma_ret(lzma_index_buffer_decode(&idx, NULL, NULL, NULL,
			NULL, 0), LZMA_PROG_ERROR);

	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			NULL, NULL, 0), LZMA_PROG_ERROR);

	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, NULL, 0), LZMA_PROG_ERROR);

	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, NULL, 0), LZMA_PROG_ERROR);

	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, &in_pos, 0), LZMA_DATA_ERROR);

	in_pos = 1;
	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, &in_pos, 0), LZMA_PROG_ERROR);
	in_pos = 0;

	// Test expected successful decode
	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, &in_pos, decode_buffer_size), LZMA_OK);

	assert_true(index_is_equal(decode_test_index, idx));

	// Test too small memlimit
	in_pos = 0;
	memlimit = 1;
	assert_lzma_ret(lzma_index_buffer_decode(&idx, &memlimit, NULL,
			decode_buffer, &in_pos, decode_buffer_size),
			LZMA_MEMLIMIT_ERROR);
	assert_uint(memlimit, <, MEMLIMIT);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);
	generate_index_decode_buffer();
	tuktest_run(test_lzma_index_memusage);
	tuktest_run(test_lzma_index_memused);
	tuktest_run(test_lzma_index_append);
	tuktest_run(test_lzma_index_stream_flags);
	tuktest_run(test_lzma_index_checks);
	tuktest_run(test_lzma_index_stream_padding);
	tuktest_run(test_lzma_index_stream_count);
	tuktest_run(test_lzma_index_block_count);
	tuktest_run(test_lzma_index_size);
	tuktest_run(test_lzma_index_stream_size);
	tuktest_run(test_lzma_index_total_size);
	tuktest_run(test_lzma_index_file_size);
	tuktest_run(test_lzma_index_uncompressed_size);
	tuktest_run(test_lzma_index_iter_init);
	tuktest_run(test_lzma_index_iter_rewind);
	tuktest_run(test_lzma_index_iter_next);
	tuktest_run(test_lzma_index_iter_locate);
	tuktest_run(test_lzma_index_cat);
	tuktest_run(test_lzma_index_dup);
	tuktest_run(test_lzma_index_encoder);
	tuktest_run(test_lzma_index_decoder);
	tuktest_run(test_lzma_index_buffer_encode);
	tuktest_run(test_lzma_index_buffer_decode);
	lzma_index_end(decode_test_index, NULL);
	return tuktest_end();
}
