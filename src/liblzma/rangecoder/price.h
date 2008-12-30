///////////////////////////////////////////////////////////////////////////////
//
/// \file       price.h
/// \brief      Probability price calculation
//
//  Copyright (C) 1999-2008 Igor Pavlov
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

#ifndef LZMA_PRICE_H
#define LZMA_PRICE_H


#define RC_MOVE_REDUCING_BITS 4
#define RC_BIT_PRICE_SHIFT_BITS 4
#define RC_PRICE_TABLE_SIZE (RC_BIT_MODEL_TOTAL >> RC_MOVE_REDUCING_BITS)

#define RC_INFINITY_PRICE (UINT32_C(1) << 30)


/// Lookup table for the inline functions defined in this file.
extern const uint8_t lzma_rc_prices[RC_PRICE_TABLE_SIZE];


static inline uint32_t
rc_bit_price(const probability prob, const uint32_t bit)
{
	return lzma_rc_prices[(prob ^ ((UINT32_C(0) - bit)
			& (RC_BIT_MODEL_TOTAL - 1))) >> RC_MOVE_REDUCING_BITS];
}


static inline uint32_t
rc_bit_0_price(const probability prob)
{
	return lzma_rc_prices[prob >> RC_MOVE_REDUCING_BITS];
}


static inline uint32_t
rc_bit_1_price(const probability prob)
{
	return lzma_rc_prices[(prob ^ (RC_BIT_MODEL_TOTAL - 1))
			>> RC_MOVE_REDUCING_BITS];
}


static inline uint32_t
rc_bittree_price(const probability *const probs,
		const uint32_t bit_levels, uint32_t symbol)
{
	uint32_t price = 0;
	symbol += UINT32_C(1) << bit_levels;

	do {
		const uint32_t bit = symbol & 1;
		symbol >>= 1;
		price += rc_bit_price(probs[symbol], bit);
	} while (symbol != 1);

	return price;
}


static inline uint32_t
rc_bittree_reverse_price(const probability *const probs,
		uint32_t bit_levels, uint32_t symbol)
{
	uint32_t price = 0;
	uint32_t model_index = 1;

	do {
		const uint32_t bit = symbol & 1;
		symbol >>= 1;
		price += rc_bit_price(probs[model_index], bit);
		model_index = (model_index << 1) + bit;
	} while (--bit_levels != 0);

	return price;
}


static inline uint32_t
rc_direct_price(const uint32_t bits)
{
	 return bits << RC_BIT_PRICE_SHIFT_BITS;
}

#endif
