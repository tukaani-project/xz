///////////////////////////////////////////////////////////////////////////////
//
/// \file       range_common.h
/// \brief      Common things for range encoder and decoder
//
//  Copyright (C) 1999-2006 Igor Pavlov
//  Copyright (C) 2006 Lasse Collin
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

#ifndef LZMA_RANGE_COMMON_H
#define LZMA_RANGE_COMMON_H

#ifdef HAVE_CONFIG_H
#	include "common.h"
#endif


///////////////
// Constants //
///////////////

#define SHIFT_BITS 8
#define TOP_BITS 24
#define TOP_VALUE (UINT32_C(1) << TOP_BITS)
#define BIT_MODEL_TOTAL_BITS 11
#define BIT_MODEL_TOTAL (UINT32_C(1) << BIT_MODEL_TOTAL_BITS)
#define MOVE_BITS 5

#define MOVE_REDUCING_BITS 2
#define BIT_PRICE_SHIFT_BITS 6


////////////
// Macros //
////////////

// Resets the probability so that both 0 and 1 have probability of 50 %
#define bit_reset(prob) \
	prob = BIT_MODEL_TOTAL >> 1

// This does the same for a complete bit tree.
// (A tree represented as an array.)
#define bittree_reset(probs, bit_levels) \
	for (uint32_t bt_i = 0; bt_i < (1 << (bit_levels)); ++bt_i) \
		bit_reset((probs)[bt_i])


//////////////////////
// Type definitions //
//////////////////////

// Bit coder speed optimization
// uint16_t is enough for probability, but usually uint32_t is faster and it
// doesn't waste too much memory. If uint64_t is fastest on 64-bit CPU, you
// probably want to use that instead of uint32_t. With uint64_t you will
// waste RAM _at maximum_ of 4.5 MiB (same for both encoding and decoding).
typedef uint32_t probability;

#endif
