///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_block_header.c
/// \brief      Tests Block Header coders
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


static uint8_t buffer[4096];
static lzma_stream strm = LZMA_STREAM_INIT;
static lzma_options_block known_options;
static lzma_options_block decoded_options;

// We want to test zero, one, and two filters in the chain.

static const lzma_options_filter filters_none[1] = {
	{
		.id = LZMA_VLI_VALUE_UNKNOWN,
		.options = NULL,
	},
};

static const lzma_options_filter filters_powerpc[2] = {
	{
		.id = LZMA_FILTER_POWERPC,
		.options = NULL,
	}, {
		.id = LZMA_VLI_VALUE_UNKNOWN,
		.options = NULL,
	},
};

static const lzma_options_delta options_delta = {
	.distance = 4,
};

static const lzma_options_filter filters_delta[3] = {
	{
		.id = LZMA_FILTER_DELTA,
		.options = (void *)(&options_delta),
	}, {
		.id = LZMA_FILTER_COPY,
		.options = NULL,
	}, {
		.id = LZMA_VLI_VALUE_UNKNOWN,
		.options = NULL,
	},
};


static bool
encode(uint32_t header_size)
{
	memcrap(buffer, sizeof(buffer));

	if (lzma_block_header_size(&known_options) != LZMA_OK)
		return true;

	if (known_options.header_size != header_size)
		return true;

	if (lzma_block_header_encode(buffer, &known_options) != LZMA_OK)
		return true;

	return false;
}


static bool
decode_ret(uint32_t header_size, lzma_ret ret_ok)
{
	memcrap(&decoded_options, sizeof(decoded_options));
	decoded_options.has_crc32 = known_options.has_crc32;

	expect(lzma_block_header_decoder(&strm, &decoded_options) == LZMA_OK);

	return decoder_loop_ret(&strm, buffer, header_size, ret_ok);
}


static bool
decode(uint32_t header_size)
{
	memcrap(&decoded_options, sizeof(decoded_options));
	decoded_options.has_crc32 = known_options.has_crc32;

	expect(lzma_block_header_decoder(&strm, &decoded_options) == LZMA_OK);

	if (decoder_loop(&strm, buffer, header_size))
		return true;

	if (known_options.has_eopm != decoded_options.has_eopm)
		return true;

	if (known_options.is_metadata != decoded_options.is_metadata)
		return true;

	if (known_options.compressed_size == LZMA_VLI_VALUE_UNKNOWN
			&& known_options.compressed_reserve != 0) {
		if (decoded_options.compressed_size != 0)
			return true;
	} else if (known_options.compressed_size
			!= decoded_options.compressed_size) {
		return true;
	}

	if (known_options.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN
			&& known_options.uncompressed_reserve != 0) {
		if (decoded_options.uncompressed_size != 0)
			return true;
	} else if (known_options.uncompressed_size
			!= decoded_options.uncompressed_size) {
		return true;
	}

	if (known_options.compressed_reserve != 0
			&& known_options.compressed_reserve
				!= decoded_options.compressed_reserve)
		return true;

	if (known_options.uncompressed_reserve != 0
			&& known_options.uncompressed_reserve
				!= decoded_options.uncompressed_reserve)
		return true;

	if (known_options.padding != decoded_options.padding)
		return true;

	return false;
}


static bool
code(uint32_t header_size)
{
	return encode(header_size) || decode(header_size);
}


static bool
helper_loop(uint32_t unpadded_size, uint32_t multiple)
{
	for (int i = 0; i <= LZMA_BLOCK_HEADER_PADDING_MAX; ++i) {
		known_options.padding = i;
		if (code(unpadded_size + i))
			return true;
	}

	for (int i = 0 - LZMA_BLOCK_HEADER_PADDING_MAX - 1;
			i <= LZMA_BLOCK_HEADER_PADDING_MAX + 1; ++i) {
		known_options.alignment = i;

		uint32_t size = unpadded_size;
		while ((size + known_options.alignment) % multiple)
			++size;

		known_options.padding = LZMA_BLOCK_HEADER_PADDING_AUTO;
		if (code(size))
			return true;

	} while (++known_options.alignment
			<= LZMA_BLOCK_HEADER_PADDING_MAX + 1);

	return false;
}


static bool
helper(uint32_t unpadded_size, uint32_t multiple)
{
	known_options.has_crc32 = false;
	known_options.is_metadata = false;
	if (helper_loop(unpadded_size, multiple))
		return true;

	known_options.has_crc32 = false;
	known_options.is_metadata = true;
	if (helper_loop(unpadded_size, multiple))
		return true;

	known_options.has_crc32 = true;
	known_options.is_metadata = false;
	if (helper_loop(unpadded_size + 4, multiple))
		return true;

	known_options.has_crc32 = true;
	known_options.is_metadata = true;
	if (helper_loop(unpadded_size + 4, multiple))
		return true;

	return false;
}


static void
test1(void)
{
	known_options = (lzma_options_block){
		.has_eopm = true,
		.compressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.compressed_reserve = 0,
		.uncompressed_reserve = 0,
	};
	memcpy(known_options.filters, filters_none, sizeof(filters_none));
	expect(!helper(2, 1));

	memcpy(known_options.filters, filters_powerpc,
			sizeof(filters_powerpc));
	expect(!helper(3, 4));

	memcpy(known_options.filters, filters_delta, sizeof(filters_delta));
	expect(!helper(5, 1));

	known_options.padding = LZMA_BLOCK_HEADER_PADDING_MAX + 1;
	expect(lzma_block_header_size(&known_options) == LZMA_PROG_ERROR);
}


static void
test2_helper(uint32_t unpadded_size, uint32_t multiple)
{
	known_options.has_eopm = true;
	known_options.compressed_size = LZMA_VLI_VALUE_UNKNOWN;
	known_options.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	known_options.compressed_reserve = 1;
	known_options.uncompressed_reserve = 1;
	expect(!helper(unpadded_size + 2, multiple));

	known_options.compressed_reserve = LZMA_VLI_BYTES_MAX;
	known_options.uncompressed_reserve = LZMA_VLI_BYTES_MAX;
	expect(!helper(unpadded_size + 18, multiple));

	known_options.compressed_size = 1234;
	known_options.uncompressed_size = 2345;
	expect(!helper(unpadded_size + 18, multiple));

	known_options.compressed_reserve = 1;
	known_options.uncompressed_reserve = 1;
	expect(lzma_block_header_size(&known_options) == LZMA_PROG_ERROR);
}


static void
test2(void)
{
	memcpy(known_options.filters, filters_none, sizeof(filters_none));
	test2_helper(2, 1);

	memcpy(known_options.filters, filters_powerpc,
			sizeof(filters_powerpc));
	test2_helper(3, 4);

	memcpy(known_options.filters, filters_delta,
			sizeof(filters_delta));
	test2_helper(5, 1);
}


static void
test3(void)
{
	known_options = (lzma_options_block){
		.has_crc32 = false,
		.has_eopm = true,
		.is_metadata = false,
		.is_metadata = false,
		.compressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.compressed_reserve = 1,
		.uncompressed_reserve = 1,
	};
	memcpy(known_options.filters, filters_none, sizeof(filters_none));

	known_options.header_size = 3;
	expect(lzma_block_header_encode(buffer, &known_options)
			== LZMA_PROG_ERROR);

	known_options.header_size = 4;
	expect(lzma_block_header_encode(buffer, &known_options) == LZMA_OK);

	known_options.header_size = 5;
	expect(lzma_block_header_encode(buffer, &known_options)
			== LZMA_PROG_ERROR);

	// NOTE: This assumes that Filter ID 0x1F is not supported. Update
	// this test to use some other ID if 0x1F becomes supported.
	known_options.filters[0].id = 0x1F;
	known_options.header_size = 5;
	expect(lzma_block_header_encode(buffer, &known_options)
			== LZMA_HEADER_ERROR);
}


static void
test4(void)
{
	known_options = (lzma_options_block){
		.has_crc32 = false,
		.has_eopm = true,
		.is_metadata = false,
		.compressed_size = 0,
		.uncompressed_size = 0,
		.compressed_reserve = LZMA_VLI_BYTES_MAX,
		.uncompressed_reserve = LZMA_VLI_BYTES_MAX,
		.padding = 0,
	};
	memcpy(known_options.filters, filters_powerpc,
			sizeof(filters_powerpc));
	expect(!code(21));

	// Reserved bits
	buffer[0] ^= 0x40;
	expect(!decode_ret(1, LZMA_HEADER_ERROR));
	buffer[0] ^= 0x40;

	buffer[1] ^= 0x40;
	expect(decode_ret(21, LZMA_HEADER_ERROR));
	buffer[1] ^= 0x40;


}


int
main(void)
{
	lzma_init();

	test1();
	test2();
	test3();
	test4();

	lzma_end(&strm);

	return 0;
}
