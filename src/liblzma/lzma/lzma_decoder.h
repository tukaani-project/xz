///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_decoder.h
/// \brief      LZMA decoder API
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

#ifndef LZMA_LZMA_DECODER_H
#define LZMA_LZMA_DECODER_H

#include "common.h"


/// Allocates and initializes LZMA decoder
extern lzma_ret lzma_lzma_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

extern uint64_t lzma_lzma_decoder_memusage(const void *options);

extern lzma_ret lzma_lzma_props_decode(
		void **options, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size);


/// \brief      Decodes the LZMA Properties byte (lc/lp/pb)
///
/// \return     true if error occorred, false on success
///
extern bool lzma_lzma_lclppb_decode(
		lzma_options_lzma *options, uint8_t byte);


#ifdef LZMA_LZ_DECODER_H
/// Allocate and setup function pointers only. This is used by LZMA1 and
/// LZMA2 decoders.
extern lzma_ret lzma_lzma_decoder_create(
		lzma_lz_decoder *lz, lzma_allocator *allocator,
		const void *opt, size_t *dict_size);
#endif

#endif
