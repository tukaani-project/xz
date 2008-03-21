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
#include "lzma_literal.h"
#include "range_common.h"


///////////////
// Constants //
///////////////

#define REP_DISTANCES 4

#define POS_SLOT_BITS 6
#define DICT_LOG_SIZE_MAX 30
#define DIST_TABLE_SIZE_MAX (DICT_LOG_SIZE_MAX * 2)
#if (UINT32_C(1) << DICT_LOG_SIZE_MAX) != LZMA_DICTIONARY_SIZE_MAX
#	error DICT_LOG_SIZE_MAX is inconsistent with LZMA_DICTIONARY_SIZE_MAX
#endif

// 2 is for speed optimization
#define LEN_TO_POS_STATES_BITS 2
#define LEN_TO_POS_STATES (1 << LEN_TO_POS_STATES_BITS)

#define MATCH_MIN_LEN 2

#define ALIGN_BITS 4
#define ALIGN_TABLE_SIZE (1 << ALIGN_BITS)
#define ALIGN_MASK (ALIGN_TABLE_SIZE - 1)

#define START_POS_MODEL_INDEX 4
#define END_POS_MODEL_INDEX 14
#define POS_MODELS (END_POS_MODEL_INDEX - START_POS_MODEL_INDEX)

#define FULL_DISTANCES_BITS (END_POS_MODEL_INDEX / 2)
#define FULL_DISTANCES (1 << FULL_DISTANCES_BITS)

#define POS_STATES_MAX (1 << LZMA_POS_BITS_MAX)


// Length coder & Length price table encoder
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)
#define LEN_SPEC_SYMBOLS (LOW_LOW_SYMBOLS + LEN_MID_LEN_SYMBOLS)
#define MATCH_MAX_LEN (MATCH_MIN_LEN + LEN_SYMBOLS - 1)

// Total number of probs in a Len Encoder
#define LEN_CODER_TOTAL_PROBS (LEN_HIGH_CODER + LEN_HIGH_SYMBOLS)

// Price table size of Len Encoder
#define LEN_PRICES (LEN_SYMBOLS << LZMA_POS_BITS_MAX)

// Special lengths used together with distance == UINT32_MAX
#define LEN_SPECIAL_EOPM MATCH_MIN_LEN
#define LEN_SPECIAL_FLUSH (LEN_SPECIAL_EOPM + 1)


// Optimal - Number of entries in the optimum array.
#define OPTS (1 << 12)


// Miscellaneous
#define INFINITY_PRICE 0x0FFFFFFF


////////////
// Macros //
////////////

#define get_len_to_pos_state(len) \
	((len) < LEN_TO_POS_STATES + MATCH_MIN_LEN \
		? (len) - MATCH_MIN_LEN \
		: LEN_TO_POS_STATES - 1)


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

#endif
