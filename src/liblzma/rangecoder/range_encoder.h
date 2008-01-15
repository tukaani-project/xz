///////////////////////////////////////////////////////////////////////////////
//
/// \file       range_encoder.h
/// \brief      Range Encoder
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

#ifndef LZMA_RANGE_ENCODER_H
#define LZMA_RANGE_ENCODER_H

#include "range_common.h"


typedef struct {
	uint64_t low;
	uint32_t range;
	uint32_t cache_size;
	uint8_t cache;
} lzma_range_encoder;


#ifdef HAVE_SMALL
/// Probability prices used by *_get_price() macros. This is initialized
/// by lzma_rc_init() and is not modified later.
extern uint32_t lzma_rc_prob_prices[BIT_MODEL_TOTAL >> MOVE_REDUCING_BITS];

/// Initializes lzma_rc_prob_prices[]. This needs to be called only once.
extern void lzma_rc_init(void);

#else
// Not building a size optimized version, so we use a precomputed
// constant table.
extern const uint32_t
lzma_rc_prob_prices[BIT_MODEL_TOTAL >> MOVE_REDUCING_BITS];

#endif


/// Resets the range encoder structure.
#define rc_reset(rc) \
do { \
	(rc).low = 0; \
	(rc).range = UINT32_MAX; \
	(rc).cache_size = 1; \
	(rc).cache = 0; \
} while (0)


//////////////////
// Bit encoding //
//////////////////

// These macros expect that the following variables are defined:
//  - lzma_range_encoder rc;
//  - uint8_t *out;
//  - size_t out_pos_local;  // Local copy of *out_pos
//  - size_t size_out;
//
// Macros pointing to these variables are also needed:
//  - uint8_t rc_buffer[]; // Don't use a pointer, must be real array!
//  - size_t rc_buffer_size;


// Combined from NRangeCoder::CEncoder::Encode()
// and NRangeCoder::CEncoder::UpdateModel().
#define bit_encode(prob, symbol) \
do { \
	probability rc_prob = prob; \
	const uint32_t rc_bound \
			= (rc.range >> BIT_MODEL_TOTAL_BITS) * rc_prob; \
	if ((symbol) == 0) { \
		rc.range = rc_bound; \
		rc_prob += (BIT_MODEL_TOTAL - rc_prob) >> MOVE_BITS; \
	} else { \
		rc.low += rc_bound; \
		rc.range -= rc_bound; \
		rc_prob -= rc_prob >> MOVE_BITS; \
	} \
	prob = rc_prob; \
	rc_normalize(); \
} while (0)


// Optimized version of bit_encode(prob, 0)
#define bit_encode_0(prob) \
do { \
	probability rc_prob = prob; \
	rc.range = (rc.range >> BIT_MODEL_TOTAL_BITS) * rc_prob; \
	rc_prob += (BIT_MODEL_TOTAL - rc_prob) >> MOVE_BITS; \
	prob = rc_prob; \
	rc_normalize(); \
} while (0)


// Optimized version of bit_encode(prob, 1)
#define bit_encode_1(prob) \
do { \
	probability rc_prob = prob; \
	const uint32_t rc_bound = (rc.range >> BIT_MODEL_TOTAL_BITS) \
			* rc_prob; \
	rc.low += rc_bound; \
	rc.range -= rc_bound; \
	rc_prob -= rc_prob >> MOVE_BITS; \
	prob = rc_prob; \
	rc_normalize(); \
} while (0)


///////////////////////
// Bit tree encoding //
///////////////////////

#define bittree_encode(probs, bit_levels, symbol) \
do { \
	uint32_t model_index = 1; \
	for (int32_t bit_index = bit_levels - 1; \
			bit_index >= 0; --bit_index) { \
		const uint32_t bit = ((symbol) >> bit_index) & 1; \
		bit_encode((probs)[model_index], bit); \
		model_index = (model_index << 1) | bit; \
	} \
} while (0)


#define bittree_reverse_encode(probs, bit_levels, symbol) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = 0; bit_index < bit_levels; ++bit_index) { \
		const uint32_t bit = ((symbol) >> bit_index) & 1; \
		bit_encode((probs)[model_index], bit); \
		model_index = (model_index << 1) | bit; \
	} \
} while (0)


/////////////////
// Direct bits //
/////////////////

#define rc_encode_direct_bits(value, num_total_bits) \
do { \
	for (int32_t rc_i = (num_total_bits) - 1; rc_i >= 0; --rc_i) { \
		rc.range >>= 1; \
		if ((((value) >> rc_i) & 1) == 1) \
			rc.low += rc.range; \
		rc_normalize(); \
	} \
} while (0)


//////////////////
// Buffer "I/O" //
//////////////////

// Calls rc_shift_low() to write out a byte if needed.
#define rc_normalize() \
do { \
	if (rc.range < TOP_VALUE) { \
		rc.range <<= SHIFT_BITS; \
		rc_shift_low(); \
	} \
} while (0)


// Flushes all the pending output.
#define rc_flush() \
	for (int32_t rc_i = 0; rc_i < 5; ++rc_i) \
		rc_shift_low()


// Writes the compressed data to next_out.
// TODO: Notation change?
//       (uint32_t)(0xFF000000)  =>  ((uint32_t)(0xFF) << TOP_BITS)
// TODO: Another notation change?
//       rc.low = (uint32_t)(rc.low) << SHIFT_BITS;
//       =>
//       rc.low &= TOP_VALUE - 1;
//       rc.low <<= SHIFT_BITS;
#define rc_shift_low() \
do { \
	if ((uint32_t)(rc.low) < (uint32_t)(0xFF000000) \
			|| (uint32_t)(rc.low >> 32) != 0) { \
		uint8_t rc_temp = rc.cache; \
		do { \
			rc_write_byte(rc_temp + (uint8_t)(rc.low >> 32)); \
			rc_temp = 0xFF; \
		} while(--rc.cache_size != 0); \
		rc.cache = (uint8_t)((uint32_t)(rc.low) >> 24); \
	} \
	++rc.cache_size; \
	rc.low = (uint32_t)(rc.low) << SHIFT_BITS; \
} while (0)


// Write one byte of compressed data to *next_out. Updates out_pos_local.
// If out_pos_local == out_size, the byte is appended to rc_buffer.
#define rc_write_byte(b) \
do { \
	if (out_pos_local == out_size) { \
		rc_buffer[rc_buffer_size++] = (uint8_t)(b); \
		assert(rc_buffer_size < sizeof(rc_buffer)); \
	} else { \
		assert(rc_buffer_size == 0); \
		out[out_pos_local++] = (uint8_t)(b); \
	} \
} while (0)


//////////////////
// Price macros //
//////////////////

// These macros expect that the following variables are defined:
//  - uint32_t lzma_rc_prob_prices;

#define bit_get_price(prob, symbol) \
	lzma_rc_prob_prices[((((prob) - (symbol)) ^ (-(symbol))) \
			& (BIT_MODEL_TOTAL - 1)) >> MOVE_REDUCING_BITS]


#define bit_get_price_0(prob) \
	lzma_rc_prob_prices[(prob) >> MOVE_REDUCING_BITS]


#define bit_get_price_1(prob) \
	lzma_rc_prob_prices[(BIT_MODEL_TOTAL - (prob)) >> MOVE_REDUCING_BITS]


static inline uint32_t
bittree_get_price(const probability *probs,
		uint32_t bit_levels, uint32_t symbol)
{
	uint32_t price = 0;
	symbol |= UINT32_C(1) << bit_levels;

	do {
		price += bit_get_price(probs[symbol >> 1], symbol & 1);
		symbol >>= 1;
	} while (symbol != 1);

	return price;
}


static inline uint32_t
bittree_reverse_get_price(const probability *probs,
		uint32_t bit_levels, uint32_t symbol)
{
	uint32_t price = 0;
	uint32_t model_index = 1;

	do {
		const uint32_t bit = symbol & 1;
		symbol >>= 1;
		price += bit_get_price(probs[model_index], bit);
		model_index = (model_index << 1) | bit;
	} while (--bit_levels != 0);

	return price;
}

#endif
