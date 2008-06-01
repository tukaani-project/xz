///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder.c
/// \brief      LZMA encoder
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


#include "lzma_encoder_private.h"
#include "fastpos.h"


/////////////
// Literal //
/////////////

static inline void
literal_normal(lzma_range_encoder *rc, probability *subcoder, uint32_t symbol)
{
	uint32_t context = 1;
	uint32_t bit_count = 8; // Bits per byte
	do {
		const uint32_t bit = (symbol >> --bit_count) & 1;
		rc_bit(rc, &subcoder[context], bit);
		context = (context << 1) | bit;
	} while (bit_count != 0);
}


static inline void
literal_matched(lzma_range_encoder *rc, probability *subcoder,
		uint32_t match_byte, uint32_t symbol)
{
	uint32_t context = 1;
	uint32_t bit_count = 8;

	do {
		uint32_t bit = (symbol >> --bit_count) & 1;
		const uint32_t match_bit = (match_byte >> bit_count) & 1;
		rc_bit(rc, &subcoder[(0x100 << match_bit) + context], bit);
		context = (context << 1) | bit;

		if (match_bit != bit) {
			// The bit from the literal being encoded and the bit
			// from the previous match differ. Finish encoding
			// as a normal literal.
			while (bit_count != 0) {
				bit = (symbol >> --bit_count) & 1;
				rc_bit(rc, &subcoder[context], bit);
				context = (context << 1) | bit;
			}

			break;
		}

	} while (bit_count != 0);
}


static inline void
literal(lzma_coder *coder)
{
	// Locate the literal byte to be encoded and the subcoder.
	const uint8_t cur_byte = coder->lz.buffer[
			coder->lz.read_pos - coder->additional_offset];
	probability *subcoder = literal_get_subcoder(coder->literal_coder,
			coder->now_pos, coder->previous_byte);

	if (is_literal_state(coder->state)) {
		// Previous LZMA-symbol was a literal. Encode a normal
		// literal without a match byte.
		literal_normal(&coder->rc, subcoder, cur_byte);
	} else {
		// Previous LZMA-symbol was a match. Use the last byte of
		// the match as a "match byte". That is, compare the bits
		// of the current literal and the match byte.
		const uint8_t match_byte = coder->lz.buffer[
				coder->lz.read_pos - coder->reps[0] - 1
				- coder->additional_offset];
		literal_matched(&coder->rc, subcoder, match_byte, cur_byte);
	}

	update_literal(coder->state);
	coder->previous_byte = cur_byte;
}


//////////////////
// Match length //
//////////////////

static inline void
length(lzma_range_encoder *rc, lzma_length_encoder *lc,
		const uint32_t pos_state, uint32_t len)
{
	assert(len <= MATCH_MAX_LEN);
	len -= MATCH_MIN_LEN;

	if (len < LEN_LOW_SYMBOLS) {
		rc_bit(rc, &lc->choice, 0);
		rc_bittree(rc, lc->low[pos_state], LEN_LOW_BITS, len);
	} else {
		rc_bit(rc, &lc->choice, 1);
		len -= LEN_LOW_SYMBOLS;

		if (len < LEN_MID_SYMBOLS) {
			rc_bit(rc, &lc->choice2, 0);
			rc_bittree(rc, lc->mid[pos_state], LEN_MID_BITS, len);
		} else {
			rc_bit(rc, &lc->choice2, 1);
			len -= LEN_MID_SYMBOLS;
			rc_bittree(rc, lc->high, LEN_HIGH_BITS, len);
		}
	}
}


///////////
// Match //
///////////

static inline void
match(lzma_coder *coder, const uint32_t pos_state,
		const uint32_t distance, const uint32_t len)
{
	update_match(coder->state);

	length(&coder->rc, &coder->match_len_encoder, pos_state, len);
	coder->prev_len_encoder = &coder->match_len_encoder;

	const uint32_t pos_slot = get_pos_slot(distance);
	const uint32_t len_to_pos_state = get_len_to_pos_state(len);
	rc_bittree(&coder->rc, coder->pos_slot_encoder[len_to_pos_state],
			POS_SLOT_BITS, pos_slot);

	if (pos_slot >= START_POS_MODEL_INDEX) {
		const uint32_t footer_bits = (pos_slot >> 1) - 1;
		const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
		const uint32_t pos_reduced = distance - base;

		if (pos_slot < END_POS_MODEL_INDEX) {
			rc_bittree_reverse(&coder->rc,
				&coder->pos_encoders[base - pos_slot - 1],
				footer_bits, pos_reduced);
		} else {
			rc_direct(&coder->rc, pos_reduced >> ALIGN_BITS,
					footer_bits - ALIGN_BITS);
			rc_bittree_reverse(
					&coder->rc, coder->pos_align_encoder,
					ALIGN_BITS, pos_reduced & ALIGN_MASK);
			++coder->align_price_count;
		}
	}

	coder->reps[3] = coder->reps[2];
	coder->reps[2] = coder->reps[1];
	coder->reps[1] = coder->reps[0];
	coder->reps[0] = distance;
	++coder->match_price_count;
}


////////////////////
// Repeated match //
////////////////////

static inline void
rep_match(lzma_coder *coder, const uint32_t pos_state,
		const uint32_t rep, const uint32_t len)
{
	if (rep == 0) {
		rc_bit(&coder->rc, &coder->is_rep0[coder->state], 0);
		rc_bit(&coder->rc,
				&coder->is_rep0_long[coder->state][pos_state],
				len != 1);
	} else {
		const uint32_t distance = coder->reps[rep];
		rc_bit(&coder->rc, &coder->is_rep0[coder->state], 1);

		if (rep == 1) {
			rc_bit(&coder->rc, &coder->is_rep1[coder->state], 0);
		} else {
			rc_bit(&coder->rc, &coder->is_rep1[coder->state], 1);
			rc_bit(&coder->rc, &coder->is_rep2[coder->state],
					rep - 2);

			if (rep == 3)
				coder->reps[3] = coder->reps[2];

			coder->reps[2] = coder->reps[1];
		}

		coder->reps[1] = coder->reps[0];
		coder->reps[0] = distance;
	}

	if (len == 1) {
		update_short_rep(coder->state);
	} else {
		length(&coder->rc, &coder->rep_len_encoder, pos_state, len);
		coder->prev_len_encoder = &coder->rep_len_encoder;
		update_long_rep(coder->state);
	}
}


//////////
// Main //
//////////

static void
encode_symbol(lzma_coder *coder, uint32_t pos, uint32_t len)
{
	const uint32_t pos_state = coder->now_pos & coder->pos_mask;

	if (len == 1 && pos == UINT32_MAX) {
		// Literal i.e. eight-bit byte
		rc_bit(&coder->rc,
				&coder->is_match[coder->state][pos_state], 0);
		literal(coder);
	} else {
		// Some type of match
		rc_bit(&coder->rc,
			&coder->is_match[coder->state][pos_state], 1);

		if (pos < REP_DISTANCES) {
			// It's a repeated match i.e. the same distance
			// has been used earlier.
			rc_bit(&coder->rc, &coder->is_rep[coder->state], 1);
			rep_match(coder, pos_state, pos, len);
		} else {
			// Normal match
			rc_bit(&coder->rc, &coder->is_rep[coder->state], 0);
			match(coder, pos_state, pos - REP_DISTANCES, len);
		}

		coder->previous_byte = coder->lz.buffer[
				coder->lz.read_pos + len - 1
				- coder->additional_offset];
	}

	assert(coder->additional_offset >= len);
	coder->additional_offset -= len;
	coder->now_pos += len;
}


static void
encode_eopm(lzma_coder *coder)
{
	const uint32_t pos_state = coder->now_pos & coder->pos_mask;
	rc_bit(&coder->rc, &coder->is_match[coder->state][pos_state], 1);
	rc_bit(&coder->rc, &coder->is_rep[coder->state], 0);
	match(coder, pos_state, UINT32_MAX, MATCH_MIN_LEN);
}


/**
 * \brief       LZMA encoder
 *
 * \return      true if end of stream was reached, false otherwise.
 */
extern bool
lzma_lzma_encode(lzma_coder *coder, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size)
{
	// Initialize the stream if no data has been encoded yet.
	if (!coder->is_initialized) {
		if (coder->lz.read_pos == coder->lz.read_limit) {
			if (coder->lz.sequence == SEQ_RUN)
				return false; // We cannot do anything.

			// We are finishing (we cannot get here when flushing).
			assert(coder->lz.write_pos == coder->lz.read_pos);
			assert(coder->lz.sequence == SEQ_FINISH);
		} else {
			// Do the actual initialization.
			uint32_t len;
			uint32_t num_distance_pairs;
			lzma_read_match_distances(coder,
					&len, &num_distance_pairs);

			encode_symbol(coder, UINT32_MAX, 1);

			assert(coder->additional_offset == 0);
		}

		// Initialization is done (except if empty file).
		coder->is_initialized = true;
	}

	// Encoding loop
	while (true) {
		// Encode pending bits, if any.
		if (rc_encode(&coder->rc, out, out_pos, out_size))
			return false;

		// Check that there is some input to process.
		if (coder->lz.read_pos >= coder->lz.read_limit) {
			// If flushing or finishing, we must keep encoding
			// until additional_offset becomes zero to make
			// all the input available at output.
			if (coder->lz.sequence == SEQ_RUN)
				return false;

			if (coder->additional_offset == 0)
				break;
		}

		assert(coder->lz.read_pos <= coder->lz.write_pos);

#ifndef NDEBUG
		if (coder->lz.sequence != SEQ_RUN) {
			assert(coder->lz.read_limit == coder->lz.write_pos);
		} else {
			assert(coder->lz.read_limit + coder->lz.keep_size_after
					== coder->lz.write_pos);
		}
#endif

		uint32_t pos;
		uint32_t len;

		// Get optimal match (repeat position and length).
		// Value ranges for pos:
		//   - [0, REP_DISTANCES): repeated match
		//   - [REP_DISTANCES, UINT32_MAX): match at (pos - REP_DISTANCES)
		//   - UINT32_MAX: not a match but a literal
		// Value ranges for len:
		//   - [MATCH_MIN_LEN, MATCH_MAX_LEN]
		if (coder->best_compression)
			lzma_get_optimum(coder, &pos, &len);
		else
			lzma_get_optimum_fast(coder, &pos, &len);

		encode_symbol(coder, pos, len);
	}

	assert(!coder->longest_match_was_found);

	if (coder->is_flushed) {
		coder->is_flushed = false;
		return true;
	}

	// We don't support encoding old LZMA streams without EOPM, and LZMA2
	// doesn't use EOPM at LZMA level.
	if (coder->write_eopm)
		encode_eopm(coder);

	rc_flush(&coder->rc);

	if (rc_encode(&coder->rc, out, out_pos, out_size)) {
		coder->is_flushed = true;
		return false;
	}

	return true;
}
