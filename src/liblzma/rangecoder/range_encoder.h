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


// Allow #including this file even if RC_TEMP_BUFFER_SIZE isn't defined.
#ifdef RC_BUFFER_SIZE
typedef struct {
	uint64_t low;
	uint32_t range;
	uint32_t cache_size;
	uint8_t cache;
	uint8_t buffer[RC_BUFFER_SIZE];
	size_t buffer_size;
} lzma_range_encoder;
#endif


/// Makes local copies of range encoder variables.
#define rc_to_local(rc) \
	uint64_t rc_low = (rc).low; \
	uint32_t rc_range = (rc).range; \
	uint32_t rc_cache_size = (rc).cache_size; \
	uint8_t rc_cache = (rc).cache; \
	uint8_t *rc_buffer = (rc).buffer; \
	size_t rc_buffer_size = (rc).buffer_size

/// Stores the local copes back to the range encoder structure.
#define rc_from_local(rc) \
do { \
	(rc).low = rc_low; \
	(rc).range = rc_range; \
	(rc).cache_size = rc_cache_size; \
	(rc).cache = rc_cache; \
	(rc).buffer_size = rc_buffer_size; \
} while (0)

/// Resets the range encoder structure.
#define rc_reset(rc) \
do { \
	(rc).low = 0; \
	(rc).range = 0xFFFFFFFF; \
	(rc).cache_size = 1; \
	(rc).cache = 0; \
	(rc).buffer_size = 0; \
} while (0)


//////////////////
// Bit encoding //
//////////////////

// These macros expect that the following variables are defined:
//  - uint64_t  rc_low;
//  - uint32_t  rc_range;
//  - uint8_t   rc_cache;
//  - uint32_t  rc_cache_size;
//  - uint8_t   *out;
//  - size_t    out_pos_local;  // Local copy of *out_pos
//  - size_t    size_out;


// Combined from NRangeCoder::CEncoder::Encode()
// and NRangeCoder::CEncoder::UpdateModel().
#define bit_encode(prob, symbol) \
do { \
	probability rc_prob = prob; \
	const uint32_t rc_bound \
			= (rc_range >> BIT_MODEL_TOTAL_BITS) * rc_prob; \
	if ((symbol) == 0) { \
		rc_range = rc_bound; \
		rc_prob += (BIT_MODEL_TOTAL - rc_prob) >> MOVE_BITS; \
	} else { \
		rc_low += rc_bound; \
		rc_range -= rc_bound; \
		rc_prob -= rc_prob >> MOVE_BITS; \
	} \
	prob = rc_prob; \
	rc_normalize(); \
} while (0)


// Optimized version of bit_encode(prob, 0)
#define bit_encode_0(prob) \
do { \
	probability rc_prob = prob; \
	rc_range = (rc_range >> BIT_MODEL_TOTAL_BITS) * rc_prob; \
	rc_prob += (BIT_MODEL_TOTAL - rc_prob) >> MOVE_BITS; \
	prob = rc_prob; \
	rc_normalize(); \
} while (0)


// Optimized version of bit_encode(prob, 1)
#define bit_encode_1(prob) \
do { \
	probability rc_prob = prob; \
	const uint32_t rc_bound = (rc_range >> BIT_MODEL_TOTAL_BITS) \
			* rc_prob; \
	rc_low += rc_bound; \
	rc_range -= rc_bound; \
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
		rc_range >>= 1; \
		if ((((value) >> rc_i) & 1) == 1) \
			rc_low += rc_range; \
		rc_normalize(); \
	} \
} while (0)


//////////////////
// Buffer "I/O" //
//////////////////

// Calls rc_shift_low() to write out a byte if needed.
#define rc_normalize() \
do { \
	if (rc_range < TOP_VALUE) { \
		rc_range <<= SHIFT_BITS; \
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
//       rc_low = (uint32_t)(rc_low) << SHIFT_BITS;
//       =>
//       rc_low &= TOP_VALUE - 1;
//       rc_low <<= SHIFT_BITS;
#define rc_shift_low() \
do { \
	if ((uint32_t)(rc_low) < (uint32_t)(0xFF000000) \
			|| (uint32_t)(rc_low >> 32) != 0) { \
		uint8_t rc_temp = rc_cache; \
		do { \
			rc_write_byte(rc_temp + (uint8_t)(rc_low >> 32)); \
			rc_temp = 0xFF; \
		} while(--rc_cache_size != 0); \
		rc_cache = (uint8_t)((uint32_t)(rc_low) >> 24); \
	} \
	++rc_cache_size; \
	rc_low = (uint32_t)(rc_low) << SHIFT_BITS; \
} while (0)


// Write one byte of compressed data to *next_out. Updates out_pos_local.
// If out_pos_local == out_size, the byte is appended to rc_buffer.
#define rc_write_byte(b) \
do { \
	if (out_pos_local == out_size) { \
		rc_buffer[rc_buffer_size++] = (uint8_t)(b); \
		assert(rc_buffer_size < RC_BUFFER_SIZE); \
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


// Adds price to price_target. TODO Optimize/Cleanup?
#define bittree_get_price(price_target, probs, bit_levels, symbol) \
do { \
	uint32_t bittree_symbol = (symbol) | (UINT32_C(1) << bit_levels); \
	while (bittree_symbol != 1) { \
		price_target += bit_get_price((probs)[bittree_symbol >> 1], \
				bittree_symbol & 1); \
		bittree_symbol >>= 1; \
	} \
} while (0)


// Adds price to price_target.
#define bittree_reverse_get_price(price_target, probs, bit_levels, symbol) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = 0; bit_index < bit_levels; ++bit_index) { \
		const uint32_t bit = ((symbol) >> bit_index) & 1; \
		price_target += bit_get_price((probs)[model_index], bit); \
		model_index = (model_index << 1) | bit; \
	} \
} while (0)


//////////////////////
// Global variables //
//////////////////////

// Probability prices used by *_get_price() macros. This is initialized
// by lzma_rc_init() and is not modified later.
extern uint32_t lzma_rc_prob_prices[BIT_MODEL_TOTAL >> MOVE_REDUCING_BITS];


///////////////
// Functions //
///////////////

/// Initializes lzma_rc_prob_prices[]. This needs to be called only once.
extern void lzma_rc_init(void);


#ifdef RC_BUFFER_SIZE
/// Flushes data from rc->temp[] to out[] as much as possible. If everything
/// cannot be flushed, returns true; false otherwise.
static inline bool
rc_flush_buffer(lzma_range_encoder *rc,
		uint8_t *out, size_t *out_pos, size_t out_size)
{
	if (rc->buffer_size > 0) {
		const size_t out_avail = out_size - *out_pos;
		if (rc->buffer_size > out_avail) {
			memcpy(out + *out_pos, rc->buffer, out_avail);
			*out_pos += out_avail;
			rc->buffer_size -= out_avail;
			memmove(rc->buffer, rc->buffer + out_avail,
					rc->buffer_size);
			return true;
		}

		memcpy(out + *out_pos, rc->buffer, rc->buffer_size);
		*out_pos += rc->buffer_size;
		rc->buffer_size = 0;
	}

	return false;
}
#endif

#endif
