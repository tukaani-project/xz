///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_check.c
/// \brief      Tests integrity checks
///
/// \todo       Add SHA256
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


// These must be specified as numbers so that the test works on EBCDIC
// systems too.
// static const uint8_t test_string[9] = "123456789";
// static const uint8_t test_unaligned[12] = "xxx123456789";
static const uint8_t test_string[9] = { 49, 50, 51, 52, 53, 54, 55, 56, 57 };
static const uint8_t test_unaligned[12]
		= { 120, 120, 120, 49, 50, 51, 52, 53, 54, 55, 56, 57 };


static void
test_crc32(void)
{
	// CRC32 is always enabled.
	assert_true(lzma_check_is_supported(LZMA_CHECK_CRC32));

	const uint32_t test_vector = 0xCBF43926;

	// Test 1
	assert_uint_eq(lzma_crc32(test_string, sizeof(test_string), 0),
			test_vector);

	// Test 2
	assert_uint_eq(lzma_crc32(test_unaligned + 3, sizeof(test_string), 0),
			test_vector);

	// Test 3
	uint32_t crc = 0;
	for (size_t i = 0; i < sizeof(test_string); ++i)
		crc = lzma_crc32(test_string + i, 1, crc);
	assert_uint_eq(crc, test_vector);
}


static void
test_crc64(void)
{
	// CRC64 can be disabled.
	if (!lzma_check_is_supported(LZMA_CHECK_CRC64))
		assert_skip("CRC64 support is disabled");

	// If CRC64 is disabled then lzma_crc64() will be missing.
	// Using an ifdef here avoids a linker error.
#ifdef HAVE_CHECK_CRC64
	const uint64_t test_vector = 0x995DC9BBDF1939FA;

	// Test 1
	assert_uint_eq(lzma_crc64(test_string, sizeof(test_string), 0),
			test_vector);

	// Test 2
	assert_uint_eq(lzma_crc64(test_unaligned + 3, sizeof(test_string), 0),
			test_vector);

	// Test 3
	uint64_t crc = 0;
	for (size_t i = 0; i < sizeof(test_string); ++i)
		crc = lzma_crc64(test_string + i, 1, crc);
	assert_uint_eq(crc, test_vector);
#endif
}


extern int
main(int argc, char **argv)
{
	tuktest_start(argc, argv);
	tuktest_run(test_crc32);
	tuktest_run(test_crc64);
	return tuktest_end();
}
