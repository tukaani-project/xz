///////////////////////////////////////////////////////////////////////////////
//
/// \file       range_encoder.c
/// \brief      Static initializations for the range encoder's prices array
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

#include "range_encoder.h"


#define NUM_BITS (BIT_MODEL_TOTAL_BITS - MOVE_REDUCING_BITS)


uint32_t lzma_rc_prob_prices[BIT_MODEL_TOTAL >> MOVE_REDUCING_BITS];


extern void
lzma_rc_init(void)
{
	// Initialize lzma_rc_prob_prices[].
	for (int i = NUM_BITS - 1; i >= 0; --i) {
		const uint32_t start = 1 << (NUM_BITS - i - 1);
		const uint32_t end = 1 << (NUM_BITS - i);

		for (uint32_t j = start; j < end; ++j) {
			lzma_rc_prob_prices[j] = (i << BIT_PRICE_SHIFT_BITS)
				+ (((end - j) << BIT_PRICE_SHIFT_BITS)
				>> (NUM_BITS - i - 1));
		}
	}

	return;
}
