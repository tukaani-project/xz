///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_private.h
/// \brief      Private definitions for LZMA encoder
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

#ifndef LZMA_LZMA_ENCODER_PRIVATE_H
#define LZMA_LZMA_ENCODER_PRIVATE_H

#include "lzma_encoder.h"
#include "lzma_common.h"
#include "lz_encoder.h"
#include "range_encoder.h"


#define move_pos(num) \
do { \
	assert((int32_t)(num) >= 0); \
	if ((num) != 0) { \
		coder->additional_offset += num; \
		coder->lz.skip(&coder->lz, num); \
	} \
} while (0)


typedef struct {
	probability choice;
	probability choice2;
	probability low[POS_STATES_MAX][LEN_LOW_SYMBOLS];
	probability mid[POS_STATES_MAX][LEN_MID_SYMBOLS];
	probability high[LEN_HIGH_SYMBOLS];

	uint32_t prices[POS_STATES_MAX][LEN_SYMBOLS];
	uint32_t table_size;
	uint32_t counters[POS_STATES_MAX];

} lzma_length_encoder;


typedef struct {
	lzma_lzma_state state;

	bool prev_1_is_char;
	bool prev_2;

	uint32_t pos_prev_2;
	uint32_t back_prev_2;

	uint32_t price;
	uint32_t pos_prev;  // pos_next;
	uint32_t back_prev;

	uint32_t backs[REP_DISTANCES];

} lzma_optimal;


struct lzma_coder_s {
	// Next coder in the chain
	lzma_next_coder next;

	// In window and match finder
	lzma_lz_encoder lz;

	// Range encoder
	lzma_range_encoder rc;

	// State
	lzma_lzma_state state;
	uint8_t previous_byte;
	uint32_t reps[REP_DISTANCES];

	// Misc
	uint32_t match_distances[MATCH_MAX_LEN * 2 + 2 + 1];
	uint32_t num_distance_pairs;
	uint32_t additional_offset;
	uint32_t now_pos; // Lowest 32 bits are enough here.
	bool best_compression;           ///< True when LZMA_MODE_BEST is used
	bool is_initialized;
	bool is_flushed;
	bool write_eopm;

	// Literal encoder
	lzma_literal_coder literal_coder;

	// Bit encoders
	probability is_match[STATES][POS_STATES_MAX];
	probability is_rep[STATES];
	probability is_rep0[STATES];
	probability is_rep1[STATES];
	probability is_rep2[STATES];
	probability is_rep0_long[STATES][POS_STATES_MAX];
	probability pos_encoders[FULL_DISTANCES - END_POS_MODEL_INDEX];

	// Bit Tree Encoders
	probability pos_slot_encoder[LEN_TO_POS_STATES][1 << POS_SLOT_BITS];
	probability pos_align_encoder[1 << ALIGN_BITS];

	// Length encoders
	lzma_length_encoder match_len_encoder;
	lzma_length_encoder rep_len_encoder;
	lzma_length_encoder *prev_len_encoder;

	// Optimal
	lzma_optimal optimum[OPTS];
	uint32_t optimum_end_index;
	uint32_t optimum_current_index;
	uint32_t longest_match_length;
	bool longest_match_was_found;

	// Prices
	uint32_t pos_slot_prices[LEN_TO_POS_STATES][DIST_TABLE_SIZE_MAX];
	uint32_t distances_prices[LEN_TO_POS_STATES][FULL_DISTANCES];
	uint32_t align_prices[ALIGN_TABLE_SIZE];
	uint32_t align_price_count;
	uint32_t dist_table_size;
	uint32_t match_price_count;

	// LZMA specific settings
	uint32_t dictionary_size;        ///< Size in bytes
	uint32_t fast_bytes;
	uint32_t pos_state_bits;
	uint32_t pos_mask;         ///< (1 << pos_state_bits) - 1
};


extern void lzma_length_encoder_update_table(lzma_length_encoder *lencoder,
		const uint32_t pos_state);

extern bool lzma_lzma_encode(lzma_coder *coder, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size);

extern void lzma_get_optimum(lzma_coder *restrict coder,
		uint32_t *restrict back_res, uint32_t *restrict len_res);

extern void lzma_get_optimum_fast(lzma_coder *restrict coder,
		uint32_t *restrict back_res, uint32_t *restrict len_res);


// NOTE: Don't add 'restrict'.
static inline void
lzma_read_match_distances(lzma_coder *coder,
		uint32_t *len_res, uint32_t *num_distance_pairs)
{
	*len_res = 0;

	coder->lz.get_matches(&coder->lz, coder->match_distances);

	*num_distance_pairs = coder->match_distances[0];

	if (*num_distance_pairs > 0) {
		*len_res = coder->match_distances[*num_distance_pairs - 1];
		assert(*len_res <= MATCH_MAX_LEN);

		if (*len_res == coder->fast_bytes) {
			uint32_t offset = *len_res - 1;
			const uint32_t distance = coder->match_distances[
					*num_distance_pairs] + 1;
			uint32_t limit = MATCH_MAX_LEN - *len_res;

			assert(offset + limit < coder->lz.keep_size_after);
			assert(coder->lz.read_pos <= coder->lz.write_pos);

			// If we are close to end of the stream, we may need
			// to limit the length of the match.
			if (coder->lz.write_pos - coder->lz.read_pos
					< offset + limit)
				limit = coder->lz.write_pos
					- (coder->lz.read_pos + offset);

			offset += coder->lz.read_pos;
			uint32_t i = 0;
			while (i < limit && coder->lz.buffer[offset + i]
					== coder->lz.buffer[
						offset + i - distance])
				++i;

			*len_res += i;
		}
	}

	++coder->additional_offset;

	return;
}

#endif
