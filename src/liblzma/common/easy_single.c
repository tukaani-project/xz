///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_single.c
/// \brief      Easy Single-Block Stream encoder initialization
//
//  Copyright (C) 2008 Lasse Collin
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

#include "easy_common.h"


extern LZMA_API lzma_ret
lzma_easy_encoder_single(lzma_stream *strm, lzma_easy_level level)
{
	lzma_options_stream opt_stream = {
		.check = LZMA_CHECK_CRC32,
		.has_crc32 = true,
		.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN,
		.alignment = 0,
	};

	if (lzma_easy_set_filters(opt_stream.filters, level))
		return LZMA_HEADER_ERROR;

	return lzma_stream_encoder_single(strm, &opt_stream);
}
