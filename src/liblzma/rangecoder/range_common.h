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

/// \brief      Type of probabilities used with range coder
///
/// This needs to be at least 12-bit integer, so uint16_t is a logical choice.
/// However, on some architecture and compiler combinations, a bigger type
/// may give better speed, because the probability variables are accessed
/// a lot. On the other hand, bigger probability type increases cache
/// footprint, since there are 2 to 14 thousand probability variables in
/// LZMA (assuming the limit of lc + lp <= 4; with lc + lp <= 12 there
/// would be about 1.5 million variables).
///
/// With malicious files, the initialization speed of the LZMA decoder can
/// become important. In that case, smaller probability variables mean that
/// there is less bytes to write to RAM, which makes initialization faster.
/// With big probability type, the initialization can become so slow that it
/// can be a problem e.g. for email servers doing virus scanning.
///
/// I will be sticking to uint16_t unless some specific architectures
/// are *much* faster (20-50 %) with uint32_t.
typedef uint16_t probability;

#endif
