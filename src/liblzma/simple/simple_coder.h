///////////////////////////////////////////////////////////////////////////////
//
/// \file       simple_coder.h
/// \brief      Wrapper for simple filters
//
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

#ifndef LZMA_SIMPLE_CODER_H
#define LZMA_SIMPLE_CODER_H

#include "common.h"


extern lzma_ret lzma_simple_x86_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_x86_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern lzma_ret lzma_simple_powerpc_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_powerpc_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern lzma_ret lzma_simple_ia64_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_ia64_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern lzma_ret lzma_simple_arm_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_arm_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern lzma_ret lzma_simple_armthumb_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_armthumb_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);


extern lzma_ret lzma_simple_sparc_encoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern lzma_ret lzma_simple_sparc_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

#endif
