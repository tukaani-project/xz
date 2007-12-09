///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_stream_flags.c
/// \brief      Tests Stream Header and Stream tail coders
//
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"


static lzma_stream_flags known_flags;
static lzma_stream_flags decoded_flags;
static uint8_t buffer[LZMA_STREAM_HEADER_SIZE + LZMA_STREAM_TAIL_SIZE];
static lzma_stream strm = LZMA_STREAM_INIT;


static bool
validate(void)
{
	if (known_flags.check != decoded_flags.check
			|| known_flags.has_crc32 != decoded_flags.has_crc32
			|| known_flags.is_multi != decoded_flags.is_multi)
		return true;

	return false;
}


static bool
test_header_decoder(size_t expected_size, lzma_ret expected_ret)
{
	memcrap(&decoded_flags, sizeof(decoded_flags));

	if (lzma_stream_header_decoder(&strm, &decoded_flags) != LZMA_OK)
		return true;

	if (coder_loop(&strm, buffer, expected_size, NULL, 0,
			expected_ret, LZMA_RUN))
		return true;

	if (expected_ret != LZMA_STREAM_END)
		return false;

	return validate();
}


static void
test_header(void)
{
	memcrap(buffer, sizeof(buffer));
	expect(lzma_stream_header_encode(buffer, &known_flags) == LZMA_OK);
	succeed(test_header_decoder(LZMA_STREAM_HEADER_SIZE, LZMA_STREAM_END));
}


static bool
test_tail_decoder(size_t expected_size, lzma_ret expected_ret)
{
	memcrap(&decoded_flags, sizeof(decoded_flags));

	if (lzma_stream_tail_decoder(&strm, &decoded_flags) != LZMA_OK)
		return true;

	if (coder_loop(&strm, buffer, expected_size, NULL, 0,
			expected_ret, LZMA_RUN))
		return true;

	if (expected_ret == LZMA_STREAM_END && validate())
		return true;

	return false;
}


static void
test_tail(void)
{
	memcrap(buffer, sizeof(buffer));
	expect(lzma_stream_tail_encode(buffer, &known_flags) == LZMA_OK);
	succeed(test_tail_decoder(LZMA_STREAM_TAIL_SIZE, LZMA_STREAM_END));
}


static void
test_encode_invalid(void)
{
	known_flags.check = LZMA_CHECK_ID_MAX + 1;
	known_flags.has_crc32 = false;
	known_flags.is_multi = false;

	expect(lzma_stream_header_encode(buffer, &known_flags)
			== LZMA_PROG_ERROR);

	expect(lzma_stream_tail_encode(buffer, &known_flags)
			== LZMA_PROG_ERROR);

	known_flags.check = (lzma_check_type)(-1);

	expect(lzma_stream_header_encode(buffer, &known_flags)
			== LZMA_PROG_ERROR);

	expect(lzma_stream_tail_encode(buffer, &known_flags)
			== LZMA_PROG_ERROR);
}


static void
test_decode_invalid(void)
{
	known_flags.check = LZMA_CHECK_NONE;
	known_flags.has_crc32 = false;
	known_flags.is_multi = false;

	expect(lzma_stream_header_encode(buffer, &known_flags) == LZMA_OK);

	// Test 1 (invalid Magic Bytes)
	buffer[5] ^= 1;
	succeed(test_header_decoder(6, LZMA_DATA_ERROR));
	buffer[5] ^= 1;

	// Test 2a (valid CRC32)
	uint32_t crc = lzma_crc32(buffer + 6, 1, 0);
	for (size_t i = 0; i < 4; ++i)
		buffer[7 + i] = crc >> (i * 8);
	succeed(test_header_decoder(LZMA_STREAM_HEADER_SIZE, LZMA_STREAM_END));

	// Test 2b (invalid Stream Flags with valid CRC32)
	buffer[6] ^= 0x20;
	crc = lzma_crc32(buffer + 6, 1, 0);
	for (size_t i = 0; i < 4; ++i)
		buffer[7 + i] = crc >> (i * 8);
	succeed(test_header_decoder(7, LZMA_HEADER_ERROR));

	// Test 3 (invalid CRC32)
	expect(lzma_stream_header_encode(buffer, &known_flags) == LZMA_OK);
	buffer[LZMA_STREAM_HEADER_SIZE - 1] ^= 1;
	succeed(test_header_decoder(LZMA_STREAM_HEADER_SIZE, LZMA_DATA_ERROR));

	// Test 4 (invalid Stream Flags)
	expect(lzma_stream_tail_encode(buffer, &known_flags) == LZMA_OK);
	buffer[0] ^= 0x40;
	succeed(test_tail_decoder(1, LZMA_HEADER_ERROR));
	buffer[0] ^= 0x40;

	// Test 5 (invalid Magic Bytes)
	buffer[2] ^= 1;
	succeed(test_tail_decoder(3, LZMA_DATA_ERROR));
}


int
main(void)
{
	lzma_init();

	// Valid headers
	for (lzma_check_type check = LZMA_CHECK_NONE;
			check <= LZMA_CHECK_ID_MAX; ++check) {
		for (int has_crc32 = 0; has_crc32 <= 1; ++has_crc32) {
			for (int is_multi = 0; is_multi <= 1; ++is_multi) {
				known_flags.check = check;
				known_flags.has_crc32 = has_crc32;
				known_flags.is_multi = is_multi;

				test_header();
				test_tail();
			}
		}
	}

	// Invalid headers
	test_encode_invalid();
	test_decode_invalid();

	lzma_end(&strm);

	return 0;
}
