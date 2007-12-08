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


/// \brief      Allocates and initializes LZMA decoder
extern lzma_ret lzma_lzma_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

/// \brief      Decodes the LZMA Properties byte (lc/lp/pb)
///
/// \return     true if error occorred, false on success
///
extern bool lzma_lzma_decode_properties(
		lzma_options_lzma *options, uint8_t byte);

// There is no public lzma_lzma_encode() because lzma_lz_encode() works
// as a wrapper for it.

#endif
