///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_decoder.c
/// \brief      LZMA decoder
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

// NOTE: If you want to keep the line length in 80 characters, set
//       tab width to 4 or less in your editor when editing this file.

#include "lzma_common.h"
#include "lzma_decoder.h"
#include "lz_decoder.h"
#include "range_decoder.h"


/// REQUIRED_IN_BUFFER_SIZE is the number of required input bytes
/// for the worst case: longest match with longest distance.
/// LZMA_IN_BUFFER_SIZE must be larger than REQUIRED_IN_BUFFER_SIZE.
/// 23 bits = 2 (match select) + 10 (len) + 6 (distance) + 4 (align)
///         + 1 (rc_normalize)
///
/// \todo       Is this correct for sure?
///
#define REQUIRED_IN_BUFFER_SIZE \
	((23 * (BIT_MODEL_TOTAL_BITS - MOVE_BITS + 1) + 26 + 9) / 8)


// Length decoders are easiest to have as macros, because they use range
// decoder macros, which use local variables rc_range and rc_code.

#define length_decode(target, len_decoder, pos_state) \
do { \
	if_bit_0(len_decoder.choice) { \
		update_bit_0(len_decoder.choice); \
		target = MATCH_MIN_LEN; \
		bittree_decode(target, len_decoder.low[pos_state], LEN_LOW_BITS); \
	} else { \
		update_bit_1(len_decoder.choice); \
		if_bit_0(len_decoder.choice2) { \
			update_bit_0(len_decoder.choice2); \
			target = MATCH_MIN_LEN + LEN_LOW_SYMBOLS; \
			bittree_decode(target, len_decoder.mid[pos_state], LEN_MID_BITS); \
		} else { \
			update_bit_1(len_decoder.choice2); \
			target = MATCH_MIN_LEN + LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS; \
			bittree_decode(target, len_decoder.high, LEN_HIGH_BITS); \
		} \
	} \
} while (0)


#define length_decode_dummy(target, len_decoder, pos_state) \
do { \
	if_bit_0(len_decoder.choice) { \
		update_bit_0_dummy(); \
		target = MATCH_MIN_LEN; \
		bittree_decode_dummy(target, \
				len_decoder.low[pos_state], LEN_LOW_BITS); \
	} else { \
		update_bit_1_dummy(); \
		if_bit_0(len_decoder.choice2) { \
			update_bit_0_dummy(); \
			target = MATCH_MIN_LEN + LEN_LOW_SYMBOLS; \
			bittree_decode_dummy(target, len_decoder.mid[pos_state], \
					LEN_MID_BITS); \
		} else { \
			update_bit_1_dummy(); \
			target = MATCH_MIN_LEN + LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS; \
			bittree_decode_dummy(target, len_decoder.high, LEN_HIGH_BITS); \
		} \
	} \
} while (0)


typedef struct {
	probability choice;
	probability choice2;
	probability low[POS_STATES_MAX][LEN_LOW_SYMBOLS];
	probability mid[POS_STATES_MAX][LEN_MID_SYMBOLS];
	probability high[LEN_HIGH_SYMBOLS];
} lzma_length_decoder;


struct lzma_coder_s {
	/// Data of the next coder, if any.
	lzma_next_coder next;

	/// Sliding output window a.k.a. dictionary a.k.a. history buffer.
	lzma_lz_decoder lz;

	// Range coder
	lzma_range_decoder rc;

	// State
	lzma_lzma_state state;
	uint32_t rep0;      ///< Distance of the latest match
	uint32_t rep1;      ///< Distance of second latest match
	uint32_t rep2;      ///< Distance of third latest match
	uint32_t rep3;      ///< Distance of fourth latest match
	uint32_t pos_bits;
	uint32_t pos_mask;
	uint32_t now_pos; // Lowest 32-bits are enough here.

	lzma_literal_coder literal_coder;

	/// If 1, it's a match. Otherwise it's a single 8-bit literal.
	probability is_match[STATES][POS_STATES_MAX];

	/// If 1, it's a repeated match. The distance is one of rep0 .. rep3.
	probability is_rep[STATES];

	/// If 0, distance of a repeated match is rep0.
	/// Otherwise check is_rep1.
	probability is_rep0[STATES];

	/// If 0, distance of a repeated match is rep1.
	/// Otherwise check is_rep2.
	probability is_rep1[STATES];

	/// If 0, distance of a repeated match is rep2. Otherwise it is rep3.
	probability is_rep2[STATES];

	/// If 1, the repeated match has length of one byte. Otherwise
	/// the length is decoded from rep_len_decoder.
	probability is_rep0_long[STATES][POS_STATES_MAX];

	probability pos_slot_decoder[LEN_TO_POS_STATES][1 << POS_SLOT_BITS];
	probability pos_decoders[FULL_DISTANCES - END_POS_MODEL_INDEX];
	probability pos_align_decoder[1 << ALIGN_BITS];

	/// Length of a match
	lzma_length_decoder match_len_decoder;

	/// Length of a repeated match.
	lzma_length_decoder rep_len_decoder;

	/// True when we have produced at least one byte of output since the
	/// beginning of the stream or the latest flush marker.
	bool has_produced_output;
};


/// \brief      Check if the next iteration of the decoder loop can be run.
///
/// \note       There must always be REQUIRED_IN_BUFFER_SIZE bytes
///             readable space!
///
static bool lzma_attribute((pure))
decode_dummy(const lzma_coder *restrict coder,
		const uint8_t *restrict in, size_t in_pos_local,
		const size_t in_size, lzma_range_decoder rc,
		uint32_t state, uint32_t rep0, const uint32_t now_pos)
{
	uint32_t rc_bound;

	do {
		const uint32_t pos_state = now_pos & coder->pos_mask;

		if_bit_0(coder->is_match[state][pos_state]) {
			// It's a literal i.e. a single 8-bit byte.

			update_bit_0_dummy();

			const probability *subcoder = literal_get_subcoder(
					coder->literal_coder, now_pos, lz_get_byte(coder->lz, 0));
			uint32_t symbol = 1;

			if (is_literal_state(state)) {
				// Decode literal without match byte.
				do {
					if_bit_0(subcoder[symbol]) {
						update_bit_0_dummy();
						symbol <<= 1;
					} else {
						update_bit_1_dummy();
						symbol = (symbol << 1) | 1;
					}
				} while (symbol < 0x100);

			} else {
				// Decode literal with match byte.
				uint32_t match_byte = lz_get_byte(coder->lz, rep0);
				uint32_t subcoder_offset = 0x100;

				do {
					match_byte <<= 1;
					const uint32_t match_bit = match_byte & subcoder_offset;
					const uint32_t subcoder_index
							= subcoder_offset + match_bit + symbol;

					if_bit_0(subcoder[subcoder_index]) {
						update_bit_0_dummy();
						symbol <<= 1;
						subcoder_offset &= ~match_bit;
					} else {
						update_bit_1_dummy();
						symbol = (symbol << 1) | 1;
						subcoder_offset &= match_bit;
					}
				} while (symbol < 0x100);
			}

			break;
		}

		update_bit_1_dummy();
		uint32_t len;

		if_bit_0(coder->is_rep[state]) {
			update_bit_0_dummy();
			length_decode_dummy(len, coder->match_len_decoder, pos_state);

			const uint32_t len_to_pos_state = get_len_to_pos_state(len);
			uint32_t pos_slot = 0;
			bittree_decode_dummy(pos_slot,
					coder->pos_slot_decoder[len_to_pos_state], POS_SLOT_BITS);
			assert(pos_slot <= 63);

			if (pos_slot >= START_POS_MODEL_INDEX) {
				uint32_t direct_bits = (pos_slot >> 1) - 1;
				assert(direct_bits >= 1 && direct_bits <= 31);
				rep0 = 2 | (pos_slot & 1);

				if (pos_slot < END_POS_MODEL_INDEX) {
					assert(direct_bits <= 5);
					rep0 <<= direct_bits;
					assert(rep0 <= 96);
					// -1 is fine, because bittree_reverse_decode()
					// starts from table index [1] (not [0]).
					assert((int32_t)(rep0 - pos_slot - 1) >= -1);
					assert((int32_t)(rep0 - pos_slot - 1) <= 82);
					// We add the result to rep0, so rep0
					// must not be part of second argument
					// of the macro.
					const int32_t offset = rep0 - pos_slot - 1;
					bittree_reverse_decode_dummy(coder->pos_decoders + offset,
							direct_bits);
				} else {
					assert(pos_slot >= 14);
					assert(direct_bits >= 6);
					direct_bits -= ALIGN_BITS;
					assert(direct_bits >= 2);
					rc_decode_direct_dummy(direct_bits);

					bittree_reverse_decode_dummy(coder->pos_align_decoder,
							ALIGN_BITS);
				}
			}

		} else {
			update_bit_1_dummy();

			if_bit_0(coder->is_rep0[state]) {
				update_bit_0_dummy();

				if_bit_0(coder->is_rep0_long[state][pos_state]) {
					update_bit_0_dummy();
					break;
				} else {
					update_bit_1_dummy();
				}

			} else {
				update_bit_1_dummy();

				if_bit_0(coder->is_rep1[state]) {
					update_bit_0_dummy();
				} else {
					update_bit_1_dummy();

					if_bit_0(coder->is_rep2[state]) {
						update_bit_0_dummy();
					} else {
						update_bit_1_dummy();
					}
				}
			}

			length_decode_dummy(len, coder->rep_len_decoder, pos_state);
		}
	} while (0);

	rc_normalize();

	return in_pos_local <= in_size;
}


static bool
decode_real(lzma_coder *restrict coder, const uint8_t *restrict in,
		size_t *restrict in_pos, size_t in_size, bool has_safe_buffer)
{
	////////////////////
	// Initialization //
	////////////////////

	if (!rc_read_init(&coder->rc, in, in_pos, in_size))
		return false;

	///////////////
	// Variables //
	///////////////

	// Making local copies of often-used variables improves both
	// speed and readability.

	// Range decoder
	rc_to_local(coder->rc);

	// State
	uint32_t state = coder->state;
	uint32_t rep0 = coder->rep0;
	uint32_t rep1 = coder->rep1;
	uint32_t rep2 = coder->rep2;
	uint32_t rep3 = coder->rep3;

	// Misc
	uint32_t now_pos = coder->now_pos;
	bool has_produced_output = coder->has_produced_output;

	// Variables derived from decoder settings
	const uint32_t pos_mask = coder->pos_mask;

	size_t in_pos_local = *in_pos; // Local copy
	size_t in_limit;
	if (in_size <= REQUIRED_IN_BUFFER_SIZE)
		in_limit = 0;
	else
		in_limit = in_size - REQUIRED_IN_BUFFER_SIZE;


	while (coder->lz.pos < coder->lz.limit
			&& (in_pos_local < in_limit || (has_safe_buffer
				&& decode_dummy(coder, in, in_pos_local, in_size,
					rc, state, rep0, now_pos)))) {

		/////////////////////
		// Actual decoding //
		/////////////////////

		const uint32_t pos_state = now_pos & pos_mask;

		if_bit_0(coder->is_match[state][pos_state]) {
			update_bit_0(coder->is_match[state][pos_state]);

			// It's a literal i.e. a single 8-bit byte.

			probability *subcoder = literal_get_subcoder(coder->literal_coder,
					now_pos, lz_get_byte(coder->lz, 0));
			uint32_t symbol = 1;

			if (is_literal_state(state)) {
				// Decode literal without match byte.
				do {
					if_bit_0(subcoder[symbol]) {
						update_bit_0(subcoder[symbol]);
						symbol <<= 1;
					} else {
						update_bit_1(subcoder[symbol]);
						symbol = (symbol << 1) | 1;
					}
				} while (symbol < 0x100);

			} else {
				// Decode literal with match byte.
				//
				// The usage of subcoder_offset allows omitting some
				// branches, which should give tiny speed improvement on
				// some CPUs. subcoder_offset gets set to zero if match_bit
				// didn't match.
				uint32_t match_byte = lz_get_byte(coder->lz, rep0);
				uint32_t subcoder_offset = 0x100;

				do {
					match_byte <<= 1;
					const uint32_t match_bit = match_byte & subcoder_offset;
					const uint32_t subcoder_index
							= subcoder_offset + match_bit + symbol;

					if_bit_0(subcoder[subcoder_index]) {
						update_bit_0(subcoder[subcoder_index]);
						symbol <<= 1;
						subcoder_offset &= ~match_bit;
					} else {
						update_bit_1(subcoder[subcoder_index]);
						symbol = (symbol << 1) | 1;
						subcoder_offset &= match_bit;
					}
				} while (symbol < 0x100);
			}

			// Put the decoded byte to the dictionary, update the
			// decoder state, and start a new decoding loop.
			coder->lz.dict[coder->lz.pos++] = (uint8_t)(symbol);
			++now_pos;
			update_literal(state);
			has_produced_output = true;
			continue;
		}

		// Instead of a new byte we are going to get a byte range
		// (distance and length) which will be repeated from our
		// output history.

		update_bit_1(coder->is_match[state][pos_state]);
		uint32_t len;

		if_bit_0(coder->is_rep[state]) {
			update_bit_0(coder->is_rep[state]);

			// Not a repeated match
			//
			// We will decode a new distance and store
			// the value to distance.

			// Decode the length of the match.
			length_decode(len, coder->match_len_decoder, pos_state);

			update_match(state);

			const uint32_t len_to_pos_state = get_len_to_pos_state(len);
			uint32_t pos_slot = 0;
			bittree_decode(pos_slot,
					coder->pos_slot_decoder[len_to_pos_state], POS_SLOT_BITS);
			assert(pos_slot <= 63);

			if (pos_slot >= START_POS_MODEL_INDEX) {
				uint32_t direct_bits = (pos_slot >> 1) - 1;
				assert(direct_bits >= 1 && direct_bits <= 30);
				uint32_t distance = 2 | (pos_slot & 1);

				if (pos_slot < END_POS_MODEL_INDEX) {
					assert(direct_bits <= 5);
					distance <<= direct_bits;
					assert(distance <= 96);
					// -1 is fine, because
					// bittree_reverse_decode()
					// starts from table index [1]
					// (not [0]).
					assert((int32_t)(distance - pos_slot - 1) >= -1);
					assert((int32_t)(distance - pos_slot - 1) <= 82);
					// We add the result to distance, so distance
					// must not be part of second argument
					// of the macro.
					const int32_t offset = distance - pos_slot - 1;
					bittree_reverse_decode(distance, coder->pos_decoders + offset,
							direct_bits);
				} else {
					assert(pos_slot >= 14);
					assert(direct_bits >= 6);
					direct_bits -= ALIGN_BITS;
					assert(direct_bits >= 2);
					rc_decode_direct(distance, direct_bits);
					distance <<= ALIGN_BITS;

					bittree_reverse_decode(distance, coder->pos_align_decoder,
							ALIGN_BITS);

					if (distance == UINT32_MAX) {
						if (len == LEN_SPECIAL_EOPM) {
							// End of Payload Marker found.
							coder->lz.eopm_detected = true;
							break;

						} else if (len == LEN_SPECIAL_FLUSH) {
							// Flush marker detected. We must have produced
							// at least one byte of output since the previous
							// flush marker or the beginning of the stream.
							// This is to prevent hanging the decoder with
							// malicious input files.
							if (!has_produced_output)
								return true;

							has_produced_output = false;

							// We know that we have enough input to call
							// this macro, because it is tested at the
							// end of decode_dummy().
							rc_normalize();

							rc_reset(rc);

							// If we don't have enough input here, we jump
							// out of the loop. Note that while there is a
							// useless call to rc_normalize(), it does nothing
							// since we have just reset the range decoder.
							if (!rc_read_init(&rc, in, &in_pos_local, in_size))
								break;

							continue;

						} else {
							return true;
						}
					}
				}

				// The latest three match distances are kept in
				// memory in case there are repeated matches.
				rep3 = rep2;
				rep2 = rep1;
				rep1 = rep0;
				rep0 = distance;

			} else {
				rep3 = rep2;
				rep2 = rep1;
				rep1 = rep0;
				rep0 = pos_slot;
			}

		} else {
			update_bit_1(coder->is_rep[state]);

			// Repeated match
			//
			// The match distance is a value that we have had
			// earlier. The latest four match distances are
			// available as rep0, rep1, rep2 and rep3. We will
			// now decode which of them is the new distance.

			if_bit_0(coder->is_rep0[state]) {
				update_bit_0(coder->is_rep0[state]);

				// The distance is rep0.

				if_bit_0(coder->is_rep0_long[state][pos_state]) {
					update_bit_0(coder->is_rep0_long[state][pos_state]);

					update_short_rep(state);

					// Repeat exactly one byte and start a new decoding loop.
					// Note that rep0 is known to have a safe value, thus we
					// don't need to check if we are wrapping the dictionary
					// when it isn't full yet.
					if (unlikely(lz_is_empty(coder->lz)))
						return true;

					coder->lz.dict[coder->lz.pos]
							= lz_get_byte(coder->lz, rep0);
					++coder->lz.pos;
					++now_pos;
					has_produced_output = true;
					continue;

				} else {
					update_bit_1(coder->is_rep0_long[state][pos_state]);

					// Repeating more than one byte at
					// distance of rep0.
				}

			} else {
				update_bit_1(coder->is_rep0[state]);

				// The distance is rep1, rep2 or rep3. Once
				// we find out which one of these three, it
				// is stored to rep0 and rep1, rep2 and rep3
				// are updated accordingly.

				uint32_t distance;

				if_bit_0(coder->is_rep1[state]) {
					update_bit_0(coder->is_rep1[state]);
					distance = rep1;
				} else {
					update_bit_1(coder->is_rep1[state]);

					if_bit_0(coder->is_rep2[state]) {
						update_bit_0(coder->is_rep2[state]);
						distance = rep2;
					} else {
						update_bit_1(coder->is_rep2[state]);
						distance = rep3;
						rep3 = rep2;
					}

					rep2 = rep1;
				}

				rep1 = rep0;
				rep0 = distance;
			}

			update_long_rep(state);

			// Decode the length of the repeated match.
			length_decode(len, coder->rep_len_decoder, pos_state);
		}


		/////////////////////////////////
		// Repeat from history buffer. //
		/////////////////////////////////

		// The length is always between these limits. There is no way
		// to trigger the algorithm to set len outside this range.
		assert(len >= MATCH_MIN_LEN);
		assert(len <= MATCH_MAX_LEN);

		now_pos += len;
		has_produced_output = true;

		// Repeat len bytes from distance of rep0.
		if (!lzma_lz_out_repeat(&coder->lz, rep0, len))
			return true;
	}

	rc_normalize();


	/////////////////////////////////
	// Update the *data structure. //
	/////////////////////////////////

	// Range decoder
	rc_from_local(coder->rc);

	// State
	coder->state = state;
	coder->rep0 = rep0;
	coder->rep1 = rep1;
	coder->rep2 = rep2;
	coder->rep3 = rep3;

	// Misc
	coder->now_pos = now_pos;
	coder->has_produced_output = has_produced_output;
	*in_pos = in_pos_local;

	return false;
}


static void
lzma_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_lz_decoder_end(&coder->lz, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_lzma_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	// LZMA can only be the last filter in the chain.
	assert(filters[1].init == NULL);

	// Validate pos_bits. Other options are validated by the
	// respective initialization functions.
	const lzma_options_lzma *options = filters[0].options;
	if (options->pos_bits > LZMA_POS_BITS_MAX)
		return LZMA_HEADER_ERROR;

	// Allocate memory for the decoder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &lzma_lz_decode;
		next->end = &lzma_decoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->lz = LZMA_LZ_DECODER_INIT;
	}

	// Store the pos_bits and calculate pos_mask.
	next->coder->pos_bits = options->pos_bits;
	next->coder->pos_mask = (1U << next->coder->pos_bits) - 1;

	// Initialize the literal decoder.
	return_if_error(lzma_literal_init(&next->coder->literal_coder,
				options->literal_context_bits,
				options->literal_pos_bits));

	// Allocate and initialize the LZ decoder.
	return_if_error(lzma_lz_decoder_reset(&next->coder->lz, allocator,
			&decode_real, options->dictionary_size,
			MATCH_MAX_LEN));

	// State
	next->coder->state = 0;
	next->coder->rep0 = 0;
	next->coder->rep1 = 0;
	next->coder->rep2 = 0;
	next->coder->rep3 = 0;
	next->coder->pos_bits = options->pos_bits;
	next->coder->pos_mask = (1 << next->coder->pos_bits) - 1;
	next->coder->now_pos = 0;

	// Range decoder
	rc_reset(next->coder->rc);

	// Bit and bittree decoders
	for (uint32_t i = 0; i < STATES; ++i) {
		for (uint32_t j = 0; j <= next->coder->pos_mask; ++j) {
			bit_reset(next->coder->is_match[i][j]);
			bit_reset(next->coder->is_rep0_long[i][j]);
		}

		bit_reset(next->coder->is_rep[i]);
		bit_reset(next->coder->is_rep0[i]);
		bit_reset(next->coder->is_rep1[i]);
		bit_reset(next->coder->is_rep2[i]);
	}

	for (uint32_t i = 0; i < LEN_TO_POS_STATES; ++i)
		bittree_reset(next->coder->pos_slot_decoder[i], POS_SLOT_BITS);

	for (uint32_t i = 0; i < FULL_DISTANCES - END_POS_MODEL_INDEX; ++i)
		bit_reset(next->coder->pos_decoders[i]);

	bittree_reset(next->coder->pos_align_decoder, ALIGN_BITS);

	// Len decoders (also bit/bittree)
	const uint32_t num_pos_states = 1 << next->coder->pos_bits;
	bit_reset(next->coder->match_len_decoder.choice);
	bit_reset(next->coder->match_len_decoder.choice2);
	bit_reset(next->coder->rep_len_decoder.choice);
	bit_reset(next->coder->rep_len_decoder.choice2);

	for (uint32_t pos_state = 0; pos_state < num_pos_states; ++pos_state) {
		bittree_reset(next->coder->match_len_decoder.low[pos_state],
				LEN_LOW_BITS);
		bittree_reset(next->coder->match_len_decoder.mid[pos_state],
				LEN_MID_BITS);

		bittree_reset(next->coder->rep_len_decoder.low[pos_state],
				LEN_LOW_BITS);
		bittree_reset(next->coder->rep_len_decoder.mid[pos_state],
				LEN_MID_BITS);
	}

	bittree_reset(next->coder->match_len_decoder.high, LEN_HIGH_BITS);
	bittree_reset(next->coder->rep_len_decoder.high, LEN_HIGH_BITS);

	next->coder->has_produced_output = false;

	return LZMA_OK;
}


extern void
lzma_lzma_decoder_uncompressed_size(
		lzma_next_coder *next, lzma_vli uncompressed_size)
{
	next->coder->lz.uncompressed_size = uncompressed_size;
	return;
}


extern bool
lzma_lzma_decode_properties(lzma_options_lzma *options, uint8_t byte)
{
	if (byte > (4 * 5 + 4) * 9 + 8)
		return true;

	// See the file format specification to understand this.
	options->pos_bits = byte / (9 * 5);
	byte -= options->pos_bits * 9 * 5;
	options->literal_pos_bits = byte / 9;
	options->literal_context_bits = byte - options->literal_pos_bits * 9;

	return options->literal_context_bits + options->literal_pos_bits
			> LZMA_LITERAL_BITS_MAX;
}
