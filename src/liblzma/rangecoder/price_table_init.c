///////////////////////////////////////////////////////////////////////////////
//
/// \file       price_table_init.c
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

#ifdef HAVE_CONFIG_H
#	include "range_encoder.h"
#endif


uint32_t lzma_rc_prices[RC_PRICE_TABLE_SIZE];


extern void
lzma_rc_init(void)
{
	for (uint32_t i = (UINT32_C(1) << RC_MOVE_REDUCING_BITS) / 2;
			i < RC_BIT_MODEL_TOTAL;
			i += (UINT32_C(1) << RC_MOVE_REDUCING_BITS)) {
		const uint32_t cycles_bits = RC_BIT_PRICE_SHIFT_BITS;
		uint32_t w = i;
		uint32_t bit_count = 0;

		for (uint32_t j = 0; j < cycles_bits; ++j) {
			w *= w;
			bit_count <<= 1;

			while (w >= (UINT32_C(1) << 16)) {
				w >>= 1;
				++bit_count;
			}
		}

		lzma_rc_prices[i >> RC_MOVE_REDUCING_BITS]
				= (RC_BIT_MODEL_TOTAL_BITS << cycles_bits)
				- 15 - bit_count;
	}

	return;
}
