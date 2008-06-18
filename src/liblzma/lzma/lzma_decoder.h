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

/// Set known uncompressed size. This is a hack needed to support
/// LZMA_Alone files that don't have EOPM.
extern void lzma_lzma_decoder_uncompressed_size(
		lzma_next_coder *next, lzma_vli uncompressed_size);

/// \brief      Decodes the LZMA Properties byte (lc/lp/pb)
///
/// \return     true if error occorred, false on success
///
extern bool lzma_lzma_decode_properties(
		lzma_options_lzma *options, uint8_t byte);

#endif
