///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma2_decoder.h
/// \brief      LZMA2 decoder
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

#ifndef LZMA_LZMA2_DECODER_H
#define LZMA_LZMA2_DECODER_H

#include "common.h"

extern lzma_ret lzma_lzma2_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern uint64_t lzma_lzma2_decoder_memusage(const void *options);

extern lzma_ret lzma_lzma2_props_decode(
		void **options, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size);

#endif
