///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_stream_flags.c
/// \brief      Tests Stream Header and Stream Footer coders
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
static uint8_t buffer[LZMA_STREAM_HEADER_SIZE];


static bool
validate(void)
{
	return !lzma_stream_flags_equal(&known_flags, &decoded_flags);
}


static bool
test_header_decoder(lzma_ret expected_ret)
{
	memcrap(&decoded_flags, sizeof(decoded_flags));

	if (lzma_stream_header_decode(&decoded_flags, buffer) != expected_ret)
		return true;

	if (expected_ret != LZMA_OK)
		return false;

	// Header doesn't have Backward Size, so make
	// lzma_stream_flags_equal() ignore it.
	decoded_flags.backward_size = LZMA_VLI_VALUE_UNKNOWN;
	return validate();
}


static void
test_header(void)
{
	memcrap(buffer, sizeof(buffer));
	expect(lzma_stream_header_encode(&known_flags, buffer) == LZMA_OK);
	succeed(test_header_decoder(LZMA_OK));
}


static bool
test_footer_decoder(lzma_ret expected_ret)
{
	memcrap(&decoded_flags, sizeof(decoded_flags));

	if (lzma_stream_footer_decode(&decoded_flags, buffer) != expected_ret)
		return true;

	if (expected_ret != LZMA_OK)
		return false;

	return validate();
}


static void
test_footer(void)
{
	memcrap(buffer, sizeof(buffer));
	expect(lzma_stream_footer_encode(&known_flags, buffer) == LZMA_OK);
	succeed(test_footer_decoder(LZMA_OK));
}


static void
test_encode_invalid(void)
{
	known_flags.check = LZMA_CHECK_ID_MAX + 1;
	known_flags.backward_size = 1024;

	expect(lzma_stream_header_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);

	expect(lzma_stream_footer_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);

	known_flags.check = (lzma_check)(-1);

	expect(lzma_stream_header_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);

	expect(lzma_stream_footer_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);

	known_flags.check = LZMA_CHECK_NONE;
	known_flags.backward_size = 0;

	// Header encoder ignores backward_size.
	expect(lzma_stream_header_encode(&known_flags, buffer) == LZMA_OK);

	expect(lzma_stream_footer_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);

	known_flags.backward_size = LZMA_VLI_VALUE_MAX;

	expect(lzma_stream_header_encode(&known_flags, buffer) == LZMA_OK);

	expect(lzma_stream_footer_encode(&known_flags, buffer)
			== LZMA_PROG_ERROR);
}


static void
test_decode_invalid(void)
{
	known_flags.check = LZMA_CHECK_NONE;
	known_flags.backward_size = 1024;

	expect(lzma_stream_header_encode(&known_flags, buffer) == LZMA_OK);

	// Test 1 (invalid Magic Bytes)
	buffer[5] ^= 1;
	succeed(test_header_decoder(LZMA_FORMAT_ERROR));
	buffer[5] ^= 1;

	// Test 2a (valid CRC32)
	uint32_t crc = lzma_crc32(buffer + 6, 2, 0);
	integer_write_32(buffer + 8, crc);
	succeed(test_header_decoder(LZMA_OK));

	// Test 2b (invalid Stream Flags with valid CRC32)
	buffer[6] ^= 0x20;
	crc = lzma_crc32(buffer + 6, 2, 0);
	integer_write_32(buffer + 8, crc);
	succeed(test_header_decoder(LZMA_HEADER_ERROR));

	// Test 3 (invalid CRC32)
	expect(lzma_stream_header_encode(&known_flags, buffer) == LZMA_OK);
	buffer[9] ^= 1;
	succeed(test_header_decoder(LZMA_DATA_ERROR));

	// Test 4 (invalid Stream Flags with valid CRC32)
	expect(lzma_stream_footer_encode(&known_flags, buffer) == LZMA_OK);
	buffer[9] ^= 0x40;
	crc = lzma_crc32(buffer + 4, 6, 0);
	integer_write_32(buffer, crc);
	succeed(test_footer_decoder(LZMA_HEADER_ERROR));

	// Test 5 (invalid Magic Bytes)
	expect(lzma_stream_footer_encode(&known_flags, buffer) == LZMA_OK);
	buffer[11] ^= 1;
	succeed(test_footer_decoder(LZMA_FORMAT_ERROR));
}


int
main(void)
{
	lzma_init();

	// Valid headers
	known_flags.backward_size = 1024;
	for (lzma_check check = LZMA_CHECK_NONE;
			check <= LZMA_CHECK_ID_MAX; ++check) {
		test_header();
		test_footer();
	}

	// Invalid headers
	test_encode_invalid();
	test_decode_invalid();

	return 0;
}
