///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_block.c
/// \brief      Tests Block coders
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


static uint8_t text[] = "Hello world!";
static uint8_t buffer[4096];
static lzma_options_block block_options;
static lzma_stream strm = LZMA_STREAM_INIT;


static void
test1(void)
{

}


int
main()
{
	lzma_init();

	block_options = (lzma_options_block){
		.check_type = LZMA_CHECK_NONE,
		.has_eopm = true,
		.has_uncompressed_size_in_footer = false,
		.has_backward_size = false,
		.handle_padding = false,
		.total_size = LZMA_VLI_VALUE_UNKNOWN,
		.compressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.header_size = 5,
	};
	block_options.filters[0].id = LZMA_VLI_VALUE_UNKNOWN;
	block_options.filters[0].options = NULL;


	lzma_end(&strm);

	return 0;
}
