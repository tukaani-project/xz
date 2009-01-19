///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder.h
/// \brief      LZMA encoder API
//
//  Copyright (C) 1999-2006 Igor Pavlov
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

#ifndef LZMA_LZMA_ENCODER_H
#define LZMA_LZMA_ENCODER_H

#include "common.h"


extern lzma_ret lzma_lzma_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern uint64_t lzma_lzma_encoder_memusage(const void *options);

extern lzma_ret lzma_lzma_props_encode(const void *options, uint8_t *out);


/// Encodes lc/lp/pb into one byte. Returns false on success and true on error.
extern bool lzma_lzma_lclppb_encode(
		const lzma_options_lzma *options, uint8_t *byte);


#ifdef LZMA_LZ_ENCODER_H

/// Initializes raw LZMA encoder; this is used by LZMA2.
extern lzma_ret lzma_lzma_encoder_create(
		lzma_coder **coder_ptr, lzma_allocator *allocator,
		const lzma_options_lzma *options, lzma_lz_options *lz_options);


/// Resets an already initialized LZMA encoder; this is used by LZMA2.
extern lzma_ret lzma_lzma_encoder_reset(
		lzma_coder *coder, const lzma_options_lzma *options);


extern lzma_ret lzma_lzma_encode(lzma_coder *restrict coder,
		lzma_mf *restrict mf, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		uint32_t read_limit);

#endif

#endif
