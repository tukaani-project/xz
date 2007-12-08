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
} lzma_range_decoder;


/// Makes local copies of range decoder variables.
#define rc_to_local(rc) \
	uint32_t rc_range = (rc).range; \
	uint32_t rc_code = (rc).code; \
	uint32_t rc_bound

/// Stores the local copes back to the range decoder structure.
#define rc_from_local(rc) \
do {\
	(rc).range = rc_range; \
	(rc).code = rc_code; \
} while (0)

/// Resets the range decoder structure.
#define rc_reset(rc) \
do { \
	(rc).range = UINT32_MAX; \
	(rc).code = 0; \
} while (0)


// All of the macros in this file expect the following variables being defined:
//  - uint32_t rc_range;
//  - uint32_t rc_code;
//  - uint32_t rc_bound;   // Temporary variable
//  - uint8_t  *in;
//  - size_t   in_pos_local; // Local alias for *in_pos


//////////////////
// Buffer "I/O" //
//////////////////

// Read the next byte of compressed data from buffer_in, if needed.
#define rc_normalize() \
do { \
	if (rc_range < TOP_VALUE) { \
		rc_range <<= SHIFT_BITS; \
		rc_code = (rc_code << SHIFT_BITS) | in[in_pos_local++]; \
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
	rc_bound = (rc_range >> BIT_MODEL_TOTAL_BITS) * (prob); \
	if (rc_code < rc_bound)


#define update_bit_0(prob) \
do { \
	rc_range = rc_bound; \
	prob += (BIT_MODEL_TOTAL - (prob)) >> MOVE_BITS; \
} while (0)


#define update_bit_1(prob) \
do { \
	rc_range -= rc_bound; \
	rc_code -= rc_bound; \
	prob -= (prob) >> MOVE_BITS; \
} while (0)


// Dummy versions don't update prob.
#define update_bit_0_dummy() \
	rc_range = rc_bound


#define update_bit_1_dummy() \
do { \
	rc_range -= rc_bound; \
	rc_code -= rc_bound; \
} while (0)


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
