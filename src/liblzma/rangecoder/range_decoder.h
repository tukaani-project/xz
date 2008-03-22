///////////////////////////////////////////////////////////////////////////////
//
/// \file       range_decoder.h
/// \brief      Range Decoder
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

#ifndef LZMA_RANGE_DECODER_H
#define LZMA_RANGE_DECODER_H

#include "range_common.h"


typedef struct {
	uint32_t range;
	uint32_t code;
	uint32_t init_bytes_left;
} lzma_range_decoder;


static inline bool
rc_read_init(lzma_range_decoder *rc, const uint8_t *restrict in,
		size_t *restrict in_pos, size_t in_size)
{
	while (rc->init_bytes_left > 0) {
		if (*in_pos == in_size)
			return false;

		rc->code = (rc->code << 8) | in[*in_pos];
		++*in_pos;
		--rc->init_bytes_left;
	}

	return true;
}


/// Makes local copies of range decoder variables.
#define rc_to_local(range_decoder) \
	lzma_range_decoder rc = range_decoder; \
	uint32_t rc_bound

/// Stores the local copes back to the range decoder structure.
#define rc_from_local(range_decoder) \
	range_decoder = rc

/// Resets the range decoder structure.
#define rc_reset(range_decoder) \
do { \
	(range_decoder).range = UINT32_MAX; \
	(range_decoder).code = 0; \
	(range_decoder).init_bytes_left = 5; \
} while (0)


// All of the macros in this file expect the following variables being defined:
//  - lzma_range_decoder range_decoder;
//  - uint32_t rc_bound;   // Temporary variable
//  - uint8_t *in;
//  - size_t in_pos_local; // Local alias for *in_pos


//////////////////
// Buffer "I/O" //
//////////////////

// Read the next byte of compressed data from buffer_in, if needed.
#define rc_normalize() \
do { \
	if (rc.range < TOP_VALUE) { \
		rc.range <<= SHIFT_BITS; \
		rc.code = (rc.code << SHIFT_BITS) | in[in_pos_local++]; \
	} \
} while (0)


//////////////////
// Bit decoding //
//////////////////

// Range decoder's DecodeBit() is splitted into three macros:
//    if_bit_0(prob) {
//        update_bit_0(prob)
//        ...
//    } else {
//        update_bit_1(prob)
//        ...
//    }

#define if_bit_0(prob) \
	rc_normalize(); \
	rc_bound = (rc.range >> BIT_MODEL_TOTAL_BITS) * (prob); \
	if (rc.code < rc_bound)


#define update_bit_0(prob) \
do { \
	rc.range = rc_bound; \
	prob += (BIT_MODEL_TOTAL - (prob)) >> MOVE_BITS; \
} while (0)


#define update_bit_1(prob) \
do { \
	rc.range -= rc_bound; \
	rc.code -= rc_bound; \
	prob -= (prob) >> MOVE_BITS; \
} while (0)


#ifdef HAVE_ARITHMETIC_RSHIFT
#	define rc_decode_direct(dest, count) \
	do { \
		rc_normalize(); \
		rc.range >>= 1; \
		rc.code -= rc.range; \
		rc_bound = (uint32_t)((int32_t)(rc.code) >> 31); \
		dest = (dest << 1) + (rc_bound + 1); \
		rc.code += rc.range & rc_bound; \
	} while (--count > 0)
#else
#	define rc_decode_direct(dest, count) \
	do { \
		rc_normalize(); \
		rc.range >>= 1; \
		rc_bound = (rc.code - rc.range) >> 31; \
		rc.code -= rc.range & (rc_bound - 1); \
		dest = ((dest) << 1) | (1 - rc_bound);\
	} while (--count > 0)
#endif


// Dummy versions don't update prob or dest.
#define update_bit_0_dummy() \
	rc.range = rc_bound


#define update_bit_1_dummy() \
do { \
	rc.range -= rc_bound; \
	rc.code -= rc_bound; \
} while (0)


#ifdef HAVE_ARITHMETIC_RSHIFT
#	define rc_decode_direct_dummy(count) \
	do { \
		rc_normalize(); \
		rc.range >>= 1; \
		rc.code -= rc.range; \
		rc.code += rc.range & ((uint32_t)((int32_t)(rc.code) >> 31)); \
	} while (--count > 0)
#else
#	define rc_decode_direct_dummy(count) \
	do { \
		rc_normalize(); \
		rc.range >>= 1; \
		rc_bound = (rc.code - rc.range) >> 31; \
		rc.code -= rc.range & (rc_bound - 1); \
	} while (--count > 0)
#endif


///////////////////////
// Bit tree decoding //
///////////////////////

#define bittree_decode(target, probs, bit_levels) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = (bit_levels); bit_index != 0; --bit_index) { \
		if_bit_0((probs)[model_index]) { \
			update_bit_0((probs)[model_index]); \
			model_index <<= 1; \
		} else { \
			update_bit_1((probs)[model_index]); \
			model_index = (model_index << 1) | 1; \
		} \
	} \
	target += model_index - (1 << bit_levels); \
} while (0)


#define bittree_reverse_decode(target, probs, bit_levels) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = 0; bit_index < bit_levels; ++bit_index) { \
		if_bit_0((probs)[model_index]) { \
			update_bit_0((probs)[model_index]); \
			model_index <<= 1; \
		} else { \
			update_bit_1((probs)[model_index]); \
			model_index = (model_index << 1) | 1; \
			target += 1 << bit_index; \
		} \
	} \
} while (0)


// Dummy versions don't update prob.
#define bittree_decode_dummy(target, probs, bit_levels) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = (bit_levels); bit_index != 0; --bit_index) { \
		if_bit_0((probs)[model_index]) { \
			update_bit_0_dummy(); \
			model_index <<= 1; \
		} else { \
			update_bit_1_dummy(); \
			model_index = (model_index << 1) | 1; \
		} \
	} \
	target += model_index - (1 << bit_levels); \
} while (0)


#define bittree_reverse_decode_dummy(probs, bit_levels) \
do { \
	uint32_t model_index = 1; \
	for (uint32_t bit_index = 0; bit_index < bit_levels; ++bit_index) { \
		if_bit_0((probs)[model_index]) { \
			update_bit_0_dummy(); \
			model_index <<= 1; \
		} else { \
			update_bit_1_dummy(); \
			model_index = (model_index << 1) | 1; \
		} \
	} \
} while (0)

#endif
