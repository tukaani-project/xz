///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_common.h
/// \brief      Private definitions common to LZMA encoder and decoder
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

#ifndef LZMA_LZMA_COMMON_H
#define LZMA_LZMA_COMMON_H

#include "common.h"
#include "range_common.h"


///////////////////
// Miscellaneous //
///////////////////

/// Maximum number of position states. A position state is the lowest pos bits
/// number of bits of the current uncompressed offset. In some places there
/// are different sets of probabilities for different pos states.
#define POS_STATES_MAX (1 << LZMA_POS_BITS_MAX)


/// Validates literal_context_bits, literal_pos_bits, and pos_bits.
static inline bool
is_lclppb_valid(const lzma_options_lzma *options)
{
	return options->literal_context_bits <= LZMA_LITERAL_CONTEXT_BITS_MAX
			&& options->literal_pos_bits
				<= LZMA_LITERAL_POS_BITS_MAX
			&& options->literal_context_bits
					+ options->literal_pos_bits
				<= LZMA_LITERAL_BITS_MAX
			&& options->pos_bits <= LZMA_POS_BITS_MAX;
}


///////////
// State //
///////////

/// This enum is used to track which events have occurred most recently and
/// in which order. This information is used to predict the next event.
///
/// Events:
///  - Literal: One 8-bit byte
///  - Match: Repeat a chunk of data at some distance
///  - Long repeat: Multi-byte match at a recently seen distance
///  - Short repeat: One-byte repeat at a recently seen distance
///
/// The event names are in from STATE_oldest_older_previous. REP means
/// either short or long repeated match, and NONLIT means any non-literal.
typedef enum {
	STATE_LIT_LIT,
	STATE_MATCH_LIT_LIT,
	STATE_REP_LIT_LIT,
	STATE_SHORTREP_LIT_LIT,
	STATE_MATCH_LIT,
	STATE_REP_LIT,
	STATE_SHORTREP_LIT,
	STATE_LIT_MATCH,
	STATE_LIT_LONGREP,
	STATE_LIT_SHORTREP,
	STATE_NONLIT_MATCH,
	STATE_NONLIT_REP,
} lzma_lzma_state;


/// Total number of states
#define STATES 12

/// The lowest 7 states indicate that the previous state was a literal.
#define LIT_STATES 7


/// Indicate that the latest state was a literal.
#define update_literal(state) \
	state = ((state) <= STATE_SHORTREP_LIT_LIT \
			? STATE_LIT_LIT \
			: ((state) <= STATE_LIT_SHORTREP \
				? (state) - 3 \
				: (state) - 6))

/// Indicate that the latest state was a match.
#define update_match(state) \
	state = ((state) < LIT_STATES ? STATE_LIT_MATCH : STATE_NONLIT_MATCH)

/// Indicate that the latest state was a long repeated match.
#define update_long_rep(state) \
	state = ((state) < LIT_STATES ? STATE_LIT_LONGREP : STATE_NONLIT_REP)

/// Indicate that the latest state was a short match.
#define update_short_rep(state) \
	state = ((state) < LIT_STATES ? STATE_LIT_SHORTREP : STATE_NONLIT_REP)

/// Test if the previous state was a literal.
#define is_literal_state(state) \
	((state) < LIT_STATES)


/////////////
// Literal //
/////////////

/// Each literal coder is divided in three sections:
///   - 0x001-0x0FF: Without match byte
///   - 0x101-0x1FF: With match byte; match bit is 0
///   - 0x201-0x2FF: With match byte; match bit is 1
///
/// Match byte is used when the previous LZMA symbol was something else than
/// a literal (that is, it was some kind of match).
#define LITERAL_CODER_SIZE 0x300

/// Maximum number of literal coders
#define LITERAL_CODERS_MAX (1 << LZMA_LITERAL_BITS_MAX)

/// Locate the literal coder for the next literal byte. The choice depends on
///   - the lowest literal_pos_bits bits of the position of the current
///     byte; and
///   - the highest literal_context_bits bits of the previous byte.
#define literal_subcoder(probs, lc, lp_mask, pos, prev_byte) \
	((probs)[(((pos) & lp_mask) << lc) + ((prev_byte) >> (8 - lc))])


static inline void
literal_init(probability (*probs)[LITERAL_CODER_SIZE],
		uint32_t literal_context_bits, uint32_t literal_pos_bits)
{
	assert(literal_context_bits + literal_pos_bits
			<= LZMA_LITERAL_BITS_MAX);

	const uint32_t coders
			= 1U << (literal_context_bits + literal_pos_bits);

	for (uint32_t i = 0; i < coders; ++i)
		for (uint32_t j = 0; j < LITERAL_CODER_SIZE; ++j)
			bit_reset(probs[i][j]);

	return;
}


//////////////////
// Match length //
//////////////////

// Minimum length of a match is two bytes.
#define MATCH_LEN_MIN 2

// Match length is encoded with 4, 5, or 10 bits.
//
// Length   Bits
//  2-9      4 = Choice=0 + 3 bits
// 10-17     5 = Choice=1 + Choice2=0 + 3 bits
// 18-273   10 = Choice=1 + Choice2=1 + 8 bits
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)

// Maximum length of a match is 273 which is a result of the encoding
// described above.
#define MATCH_LEN_MAX (MATCH_LEN_MIN + LEN_SYMBOLS - 1)


////////////////////
// Match distance //
////////////////////

// Different set of probabilities is used for match distances that have very
// short match length: Lengths of 2, 3, and 4 bytes have a separate set of
// probabilities for each length. The matches with longer length use a shared
// set of probabilities.
#define LEN_TO_POS_STATES 4

// Macro to get the index of the appropriate probability array.
#define get_len_to_pos_state(len) \
	((len) < LEN_TO_POS_STATES + MATCH_LEN_MIN \
		? (len) - MATCH_LEN_MIN \
		: LEN_TO_POS_STATES - 1)

// The highest two bits of a match distance (pos slot) are encoded using six
// bits. See fastpos.h for more explanation.
#define POS_SLOT_BITS 6
#define POS_SLOTS (1 << POS_SLOT_BITS)

// Match distances up to 127 are fully encoded using probabilities. Since
// the highest two bits (pos slot) are always encoded using six bits, the
// distances 0-3 don't need any additional bits to encode, since the pos
// slot itself is the same as the actual distance. START_POS_MODEL_INDEX
// indicates the first pos slot where at least one additional bit is needed.
#define START_POS_MODEL_INDEX 4

// Match distances greater than 127 are encoded in three pieces:
//   - pos slot: the highest two bits
//   - direct bits: 2-26 bits below the highest two bits
//   - alignment bits: four lowest bits
//
// Direct bits don't use any probabilities.
//
// The pos slot value of 14 is for distances 128-191 (see the table in
// fastpos.h to understand why).
#define END_POS_MODEL_INDEX 14

// Seven-bit distances use the full FIXME
#define FULL_DISTANCES_BITS (END_POS_MODEL_INDEX / 2)
#define FULL_DISTANCES (1 << FULL_DISTANCES_BITS)

// For match distances greater than 127, only the highest two bits and the
// lowest four bits (alignment) is encoded using probabilities.
#define ALIGN_BITS 4
#define ALIGN_TABLE_SIZE (1 << ALIGN_BITS)
#define ALIGN_MASK (ALIGN_TABLE_SIZE - 1)

// LZMA remembers the four most recent match distances. Reusing these distances
// tends to take less space than re-encoding the actual distance value.
#define REP_DISTANCES 4

#endif
