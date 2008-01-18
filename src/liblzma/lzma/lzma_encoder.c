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


////////////
// Macros //
////////////

// These are as macros mostly because they use local range encoder variables.

#define literal_encode(subcoder, symbol) \
do { \
	uint32_t context = 1; \
	int i = 8; \
	do { \
		--i; \
		const uint32_t bit = ((symbol) >> i) & 1; \
		bit_encode(subcoder[context], bit); \
		context = (context << 1) | bit; \
	} while (i != 0); \
} while (0)


#define literal_encode_matched(subcoder, match_byte, symbol) \
do { \
	uint32_t context = 1; \
	int i = 8; \
	do { \
		--i; \
		uint32_t bit = ((symbol) >> i) & 1; \
		const uint32_t match_bit = ((match_byte) >> i) & 1; \
		const uint32_t subcoder_index = 0x100 + (match_bit << 8) + context; \
		bit_encode(subcoder[subcoder_index], bit); \
		context = (context << 1) | bit; \
		if (match_bit != bit) { \
			while (i != 0) { \
				--i; \
				bit = ((symbol) >> i) & 1; \
				bit_encode(subcoder[context], bit); \
				context = (context << 1) | bit; \
			} \
			break; \
		} \
	} while (i != 0); \
} while (0)


#define length_encode(length_encoder, symbol, pos_state, update_price) \
do { \
	\
	if ((symbol) < LEN_LOW_SYMBOLS) { \
		bit_encode_0((length_encoder).choice); \
		bittree_encode((length_encoder).low[pos_state], \
				LEN_LOW_BITS, symbol); \
	} else { \
		bit_encode_1((length_encoder).choice); \
		if ((symbol) < LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS) { \
			bit_encode_0((length_encoder).choice2); \
			bittree_encode((length_encoder).mid[pos_state], \
					LEN_MID_BITS, \
					(symbol) - LEN_LOW_SYMBOLS); \
		} else { \
			bit_encode_1((length_encoder).choice2); \
			bittree_encode((length_encoder).high, LEN_HIGH_BITS, \
					(symbol) - LEN_LOW_SYMBOLS \
					- LEN_MID_SYMBOLS); \
		} \
	} \
	if (update_price) \
		if (--(length_encoder).counters[pos_state] == 0) \
			lzma_length_encoder_update_table(&(length_encoder), pos_state); \
} while (0)


///////////////
// Functions //
///////////////

/// \brief      Updates price table of the length encoder
///
/// Like all the other prices in LZMA, these are used by lzma_get_optimum().
///
extern void
lzma_length_encoder_update_table(lzma_length_encoder *lencoder,
		const uint32_t pos_state)
{
	const uint32_t num_symbols = lencoder->table_size;
	const uint32_t a0 = bit_get_price_0(lencoder->choice);
	const uint32_t a1 = bit_get_price_1(lencoder->choice);
	const uint32_t b0 = a1 + bit_get_price_0(lencoder->choice2);
	const uint32_t b1 = a1 + bit_get_price_1(lencoder->choice2);

	uint32_t *prices = lencoder->prices[pos_state];
	uint32_t i = 0;

	for (i = 0; i < num_symbols && i < LEN_LOW_SYMBOLS; ++i)
		prices[i] = a0 + bittree_get_price(lencoder->low[pos_state],
				LEN_LOW_BITS, i);

	for (; i < num_symbols && i < LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS; ++i)
		prices[i] = b0 + bittree_get_price(lencoder->mid[pos_state],
				LEN_MID_BITS, i - LEN_LOW_SYMBOLS);

	for (; i < num_symbols; ++i)
		prices[i] = b1 + bittree_get_price(
				lencoder->high, LEN_HIGH_BITS,
				i - LEN_LOW_SYMBOLS - LEN_MID_SYMBOLS);

	lencoder->counters[pos_state] = num_symbols;

	return;
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
#define rc_buffer coder->lz.temp
#define rc_buffer_size coder->lz.temp_size

	// Local copies
	lzma_range_encoder rc = coder->rc;
	size_t out_pos_local = *out_pos;
	const uint32_t pos_mask = coder->pos_mask;
	const bool best_compression = coder->best_compression;

	// Initialize the stream if no data has been encoded yet.
	if (!coder->is_initialized) {
		if (coder->lz.read_pos == coder->lz.read_limit) {
			assert(coder->lz.write_pos == coder->lz.read_pos);
			assert(coder->lz.sequence == SEQ_FINISH);
		} else {
			// Do the actual initialization.
			uint32_t len;
			uint32_t num_distance_pairs;
			lzma_read_match_distances(coder, &len, &num_distance_pairs);

			bit_encode_0(coder->is_match[coder->state][0]);
			update_char(coder->state);

			const uint8_t cur_byte = coder->lz.buffer[
					coder->lz.read_pos - coder->additional_offset];
			probability *subcoder = literal_get_subcoder(coder->literal_coder,
					coder->now_pos, coder->previous_byte);
			literal_encode(subcoder, cur_byte);

			coder->previous_byte = cur_byte;
			--coder->additional_offset;
			++coder->now_pos;

			assert(coder->additional_offset == 0);
		}

		// Initialization is done (except if empty file).
		coder->is_initialized = true;
	}

	// Encoding loop
	while (true) {
		// Check that there is free output space.
		if (out_pos_local == out_size)
			break;

		assert(rc_buffer_size == 0);

		// Check that there is some input to process.
		if (coder->lz.read_pos >= coder->lz.read_limit) {
			// If flushing or finishing, we must keep encoding
			// until additional_offset becomes zero to make
			// all the input available at output.
			if (coder->lz.sequence == SEQ_RUN
					|| coder->additional_offset == 0)
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

		const uint32_t pos_state = coder->now_pos & pos_mask;

		uint32_t pos;
		uint32_t len;

		// Get optimal match (repeat position and length).
		// Value ranges for pos:
		//   - [0, REP_DISTANCES): repeated match
		//   - [REP_DISTANCES, UINT32_MAX): match at (pos - REP_DISTANCES)
		//   - UINT32_MAX: not a match but a literal
		// Value ranges for len:
		//   - [MATCH_MIN_LEN, MATCH_MAX_LEN]
		if (best_compression)
			lzma_get_optimum(coder, &pos, &len);
		else
			lzma_get_optimum_fast(coder, &pos, &len);

		if (len == 1 && pos == UINT32_MAX) {
			// It's a literal.
			bit_encode_0(coder->is_match[coder->state][pos_state]);

			const uint8_t cur_byte = coder->lz.buffer[
					coder->lz.read_pos - coder->additional_offset];
			probability *subcoder = literal_get_subcoder(coder->literal_coder,
					coder->now_pos, coder->previous_byte);

			if (is_char_state(coder->state)) {
				literal_encode(subcoder, cur_byte);
			} else {
				const uint8_t match_byte = coder->lz.buffer[
						coder->lz.read_pos
						- coder->rep_distances[0] - 1
						- coder->additional_offset];
				literal_encode_matched(subcoder, match_byte, cur_byte);
			}

			update_char(coder->state);
			coder->previous_byte = cur_byte;

		} else {
			// It's a match.
			bit_encode_1(coder->is_match[coder->state][pos_state]);

			if (pos < REP_DISTANCES) {
				// It's a repeated match i.e. the same distance
				// has been used earlier.
				bit_encode_1(coder->is_rep[coder->state]);

				if (pos == 0) {
					bit_encode_0(coder->is_rep0[coder->state]);
					const uint32_t symbol = (len == 1) ? 0 : 1;
					bit_encode(coder->is_rep0_long[coder->state][pos_state],
							symbol);
				} else {
					const uint32_t distance = coder->rep_distances[pos];
					bit_encode_1(coder->is_rep0[coder->state]);

					if (pos == 1) {
						bit_encode_0(coder->is_rep1[coder->state]);
					} else {
						bit_encode_1(coder->is_rep1[coder->state]);
						bit_encode(coder->is_rep2[coder->state], pos - 2);

						if (pos == 3)
							coder->rep_distances[3] = coder->rep_distances[2];

						coder->rep_distances[2] = coder->rep_distances[1];
					}

					coder->rep_distances[1] = coder->rep_distances[0];
					coder->rep_distances[0] = distance;
				}

				if (len == 1) {
					update_short_rep(coder->state);
				} else {
					length_encode(coder->rep_match_len_encoder,
							len - MATCH_MIN_LEN, pos_state,
							best_compression);
					update_rep(coder->state);
				}

			} else {
				bit_encode_0(coder->is_rep[coder->state]);
				update_match(coder->state);
				length_encode(coder->len_encoder, len - MATCH_MIN_LEN,
						pos_state, best_compression);
				pos -= REP_DISTANCES;

				const uint32_t pos_slot = get_pos_slot(pos);
				const uint32_t len_to_pos_state = get_len_to_pos_state(len);
				bittree_encode(coder->pos_slot_encoder[len_to_pos_state],
						POS_SLOT_BITS, pos_slot);

				if (pos_slot >= START_POS_MODEL_INDEX) {
					const uint32_t footer_bits = (pos_slot >> 1) - 1;
					const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
					const uint32_t pos_reduced = pos - base;

					if (pos_slot < END_POS_MODEL_INDEX) {
						bittree_reverse_encode(
								coder->pos_encoders + base - pos_slot - 1,
								footer_bits, pos_reduced);
					} else {
						rc_encode_direct_bits(pos_reduced >> ALIGN_BITS,
								footer_bits - ALIGN_BITS);
						bittree_reverse_encode(coder->pos_align_encoder,
								ALIGN_BITS, pos_reduced & ALIGN_MASK);
						++coder->align_price_count;
					}
				}

				coder->rep_distances[3] = coder->rep_distances[2];
				coder->rep_distances[2] = coder->rep_distances[1];
				coder->rep_distances[1] = coder->rep_distances[0];
				coder->rep_distances[0] = pos;
				++coder->match_price_count;
			}

			coder->previous_byte = coder->lz.buffer[
					coder->lz.read_pos + len - 1
					- coder->additional_offset];
		}

		assert(coder->additional_offset >= len);
		coder->additional_offset -= len;
		coder->now_pos += len;
	}

	// Check if everything is done.
	bool all_done = false;
	if (coder->lz.sequence != SEQ_RUN
			&& coder->lz.read_pos == coder->lz.write_pos
			&& coder->additional_offset == 0) {
		if (coder->lz.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN
				|| coder->lz.sequence == SEQ_FLUSH) {
			// Write special marker: flush marker or end of payload
			// marker. Both are encoded as a match with distance of
			// UINT32_MAX. The match length codes the type of the marker.
			const uint32_t pos_state = coder->now_pos & pos_mask;
			bit_encode_1(coder->is_match[coder->state][pos_state]);
			bit_encode_0(coder->is_rep[coder->state]);
			update_match(coder->state);

			const uint32_t len = coder->lz.sequence == SEQ_FLUSH
					? LEN_SPECIAL_FLUSH : LEN_SPECIAL_EOPM;
			length_encode(coder->len_encoder, len - MATCH_MIN_LEN,
					pos_state, best_compression);

			const uint32_t pos_slot = (1 << POS_SLOT_BITS) - 1;
			const uint32_t len_to_pos_state = get_len_to_pos_state(len);
			bittree_encode(coder->pos_slot_encoder[len_to_pos_state],
					POS_SLOT_BITS, pos_slot);

			const uint32_t footer_bits = 30;
			const uint32_t pos_reduced
					= (UINT32_C(1) << footer_bits) - 1;
			rc_encode_direct_bits(pos_reduced >> ALIGN_BITS,
					footer_bits - ALIGN_BITS);

			bittree_reverse_encode(coder->pos_align_encoder, ALIGN_BITS,
					pos_reduced & ALIGN_MASK);
		}

		// Flush the last bytes of compressed data from
		// the range coder to the output buffer.
		rc_flush();

		rc_reset(rc);

		// All done. Note that some output bytes might be
		// pending in coder->lz.temp. lzma_lz_encode() will
		// take care of those bytes.
		all_done = true;
	}

	// Store local variables back to *coder.
	coder->rc = rc;
	*out_pos = out_pos_local;

	return all_done;
}
