///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma2_encoder.h
/// \brief      LZMA2 encoder
//
//  Copyright (C) 1999-2008 Igor Pavlov
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

#ifndef LZMA_LZMA2_ENCODER_H
#define LZMA_LZMA2_ENCODER_H

#include "common.h"


/// Maximum number of bytes of actual data per chunk (no headers)
#define LZMA2_CHUNK_MAX (UINT32_C(1) << 16)

/// Maximum uncompressed size of LZMA chunk (no headers)
#define LZMA2_UNCOMPRESSED_MAX (UINT32_C(1) << 21)

/// Maximum size of LZMA2 headers
#define LZMA2_HEADER_MAX 6

/// Size of a header for uncompressed chunk
#define LZMA2_HEADER_UNCOMPRESSED 3


extern lzma_ret lzma_lzma2_encoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters);

extern uint64_t lzma_lzma2_encoder_memusage(const void *options);

extern lzma_ret lzma_lzma2_props_encode(const void *options, uint8_t *out);

#endif
