///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_getoptimum.c
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


// "Would you love the monster code?
//  Could you understand beauty of the beast?"
//      --Adapted from Lordi's "Would you love a monster man".


#include "lzma_encoder_private.h"
#include "fastpos.h"


#define length_get_price(length_encoder, symbol, pos_state) \
	(length_encoder).prices[pos_state][symbol]


#define get_rep_len_1_price(state, pos_state) \
	bit_get_price_0(coder->is_rep0[state]) \
			+ bit_get_price_0(coder->is_rep0_long[state][pos_state])


// Adds to price_target.
#define get_pure_rep_price(price_target, rep_index, state, pos_state) \
do { \
	if ((rep_index) == 0) { \
		price_target += bit_get_price_0(coder->is_rep0[state]); \
		price_target += bit_get_price_1( \
				coder->is_rep0_long[state][pos_state]); \
	} else { \
		price_target += bit_get_price_1(coder->is_rep0[state]); \
		if ((rep_index) == 1) { \
			price_target += bit_get_price_0(coder->is_rep1[state]); \
		} else { \
			price_target += bit_get_price_1(coder->is_rep1[state]); \
			price_target += bit_get_price( \
					coder->is_rep2[state], (rep_index) - 2); \
		} \
	} \
} while (0)


// Adds to price_target.
#define get_rep_price(price_target, rep_index, len, state, pos_state) \
do { \
	get_pure_rep_price(price_target, rep_index, state, pos_state); \
	price_target += length_get_price(coder->rep_len_encoder, \
			(len) - MATCH_MIN_LEN, pos_state); \
} while (0)


// Adds to price_target.
#define get_pos_len_price(price_target, pos, len, pos_state) \
do { \
	const uint32_t len_to_pos_state_tmp = get_len_to_pos_state(len); \
	if ((pos) < FULL_DISTANCES) { \
		price_target += distances_prices[len_to_pos_state_tmp][pos]; \
	} else { \
		price_target \
				+= pos_slot_prices[len_to_pos_state_tmp][get_pos_slot_2(pos)] \
				+ align_prices[(pos) & ALIGN_MASK]; \
	} \
	price_target += length_get_price( \
			coder->match_len_encoder, (len) - MATCH_MIN_LEN, pos_state); \
} while (0)


// Three macros to manipulate lzma_optimal structures:
#define make_as_char(opt) \
do { \
	(opt).back_prev = UINT32_MAX; \
	(opt).prev_1_is_char = false; \
} while (0)


#define make_as_short_rep(opt) \
do { \
	(opt).back_prev = 0; \
	(opt).prev_1_is_char = false; \
} while (0)


#define is_short_rep(opt) \
	((opt).back_prev == 0)


static void
fill_distances_prices(lzma_coder *coder)
{
	uint32_t temp_prices[FULL_DISTANCES];

	for (uint32_t i = START_POS_MODEL_INDEX; i < FULL_DISTANCES; ++i) {
		const uint32_t pos_slot = get_pos_slot(i);
		const uint32_t footer_bits = ((pos_slot >> 1) - 1);
		const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
		temp_prices[i] = bittree_reverse_get_price(
				coder->pos_encoders + base - pos_slot - 1,
				footer_bits, i - base);
	}

	const uint32_t dist_table_size = coder->dist_table_size;

	for (uint32_t len_to_pos_state = 0;
			len_to_pos_state < LEN_TO_POS_STATES;
			++len_to_pos_state) {

		const probability *encoder = coder->pos_slot_encoder[len_to_pos_state];
		uint32_t *pos_slot_prices = coder->pos_slot_prices[len_to_pos_state];

		for (uint32_t pos_slot = 0;
				pos_slot < dist_table_size;
				++pos_slot) {
			pos_slot_prices[pos_slot] = bittree_get_price(encoder,
					POS_SLOT_BITS, pos_slot);
		}

		for (uint32_t pos_slot = END_POS_MODEL_INDEX;
				pos_slot < dist_table_size;
				++pos_slot)
			pos_slot_prices[pos_slot] += (((pos_slot >> 1) - 1)
					- ALIGN_BITS) << BIT_PRICE_SHIFT_BITS;


		uint32_t *distances_prices
				= coder->distances_prices[len_to_pos_state];

		uint32_t i;
		for (i = 0; i < START_POS_MODEL_INDEX; ++i)
			distances_prices[i] = pos_slot_prices[i];

		for (; i < FULL_DISTANCES; ++i)
			distances_prices[i] = pos_slot_prices[get_pos_slot(i)]
					+ temp_prices[i];
	}

	coder->match_price_count = 0;

	return;
}


static void
fill_align_prices(lzma_coder *coder)
{
	for (uint32_t i = 0; i < ALIGN_TABLE_SIZE; ++i)
		coder->align_prices[i] = bittree_reverse_get_price(
				coder->pos_align_encoder, ALIGN_BITS, i);

	coder->align_price_count = 0;
	return;
}


// The first argument is a pointer returned by literal_get_subcoder().
static uint32_t
literal_get_price(const probability *encoders, const bool match_mode,
		const uint8_t match_byte, const uint8_t symbol)
{
	uint32_t price = 0;
	uint32_t context = 1;
	int i = 8;

	if (match_mode) {
		do {
			--i;
			const uint32_t match_bit = (match_byte >> i) & 1;
			const uint32_t bit = (symbol >> i) & 1;
			const uint32_t subcoder_index
					= 0x100 + (match_bit << 8) + context;

			price += bit_get_price(encoders[subcoder_index], bit);
			context = (context << 1) | bit;

			if (match_bit != bit)
				break;

		} while (i != 0);
	}

	while (i != 0) {
		--i;
		const uint32_t bit = (symbol >> i) & 1;
		price += bit_get_price(encoders[context], bit);
		context = (context << 1) | bit;
	}

	return price;
}


static void
backward(lzma_coder *restrict coder, uint32_t *restrict len_res,
		uint32_t *restrict back_res, uint32_t cur)
{
	coder->optimum_end_index = cur;

	uint32_t pos_mem = coder->optimum[cur].pos_prev;
	uint32_t back_mem = coder->optimum[cur].back_prev;

	do {
		if (coder->optimum[cur].prev_1_is_char) {
			make_as_char(coder->optimum[pos_mem]);
			coder->optimum[pos_mem].pos_prev = pos_mem - 1;

			if (coder->optimum[cur].prev_2) {
				coder->optimum[pos_mem - 1].prev_1_is_char = false;
				coder->optimum[pos_mem - 1].pos_prev
						= coder->optimum[cur].pos_prev_2;
				coder->optimum[pos_mem - 1].back_prev
						= coder->optimum[cur].back_prev_2;
			}
		}

		uint32_t pos_prev = pos_mem;
		uint32_t back_cur = back_mem;

		back_mem = coder->optimum[pos_prev].back_prev;
		pos_mem = coder->optimum[pos_prev].pos_prev;

		coder->optimum[pos_prev].back_prev = back_cur;
		coder->optimum[pos_prev].pos_prev = cur;
		cur = pos_prev;

	} while (cur != 0);

	coder->optimum_current_index = coder->optimum[0].pos_prev;
	*len_res = coder->optimum[0].pos_prev;
	*back_res = coder->optimum[0].back_prev;

	return;
}


extern void
lzma_get_optimum(lzma_coder *restrict coder,
		uint32_t *restrict back_res, uint32_t *restrict len_res)
{
	// Update the price tables. In the C++ LZMA SDK 4.42 this was done in both
	// initialization function and in the main loop. In liblzma they were
	// moved into this single place.
	if (coder->additional_offset == 0) {
		if (coder->match_price_count >= (1 << 7))
			fill_distances_prices(coder);

		if (coder->align_price_count >= ALIGN_TABLE_SIZE)
			fill_align_prices(coder);
	}


	if (coder->optimum_end_index != coder->optimum_current_index) {
		*len_res = coder->optimum[coder->optimum_current_index].pos_prev
				- coder->optimum_current_index;
		*back_res = coder->optimum[coder->optimum_current_index].back_prev;
		coder->optimum_current_index = coder->optimum[
				coder->optimum_current_index].pos_prev;
		return;
	}

	coder->optimum_current_index = 0;
	coder->optimum_end_index = 0;


	const uint32_t fast_bytes = coder->fast_bytes;
	uint32_t *match_distances = coder->match_distances;

	uint32_t len_main;
	uint32_t num_distance_pairs;

	if (!coder->longest_match_was_found) {
		lzma_read_match_distances(coder, &len_main, &num_distance_pairs);
	} else {
		len_main = coder->longest_match_length;
		num_distance_pairs = coder->num_distance_pairs;
		coder->longest_match_was_found = false;
	}


	const uint8_t *buf = coder->lz.buffer + coder->lz.read_pos - 1;
	uint32_t num_available_bytes
			= coder->lz.write_pos - coder->lz.read_pos + 1;
	if (num_available_bytes < 2) {
		*back_res = UINT32_MAX;
		*len_res = 1;
		return;
	}

	if (num_available_bytes > MATCH_MAX_LEN)
		num_available_bytes = MATCH_MAX_LEN;


	uint32_t reps[REP_DISTANCES];
	uint32_t rep_lens[REP_DISTANCES];
	uint32_t rep_max_index = 0;

	for (uint32_t i = 0; i < REP_DISTANCES; ++i) {
		reps[i] = coder->rep_distances[i];
		const uint32_t back_offset = reps[i] + 1;

		if (buf[0] != *(buf - back_offset)
				|| buf[1] != *(buf + 1 - back_offset)) {
			rep_lens[i] = 0;
			continue;
		}

		uint32_t len_test;
		for (len_test = 2; len_test < num_available_bytes
				&& buf[len_test] == *(buf + len_test - back_offset);
				++len_test) ;

		rep_lens[i] = len_test;
		if (len_test > rep_lens[rep_max_index])
			rep_max_index = i;
	}

	if (rep_lens[rep_max_index] >= fast_bytes) {
		*back_res = rep_max_index;
		*len_res = rep_lens[rep_max_index];
		move_pos(*len_res - 1);
		return;
	}


	if (len_main >= fast_bytes) {
		*back_res = match_distances[num_distance_pairs] + REP_DISTANCES;
		*len_res = len_main;
		move_pos(len_main - 1);
		return;
	}

	uint8_t current_byte = *buf;
	uint8_t match_byte = *(buf - reps[0] - 1);

	if (len_main < 2 && current_byte != match_byte
			&& rep_lens[rep_max_index] < 2) {
		*back_res = UINT32_MAX;
		*len_res = 1;
		return;
	}

	const uint32_t pos_mask = coder->pos_mask;

	coder->optimum[0].state = coder->state;

	uint32_t position = coder->now_pos;
	uint32_t pos_state = (position & pos_mask);

	coder->optimum[1].price = bit_get_price_0(
				coder->is_match[coder->state][pos_state])
			+ literal_get_price(
				literal_get_subcoder(coder->literal_coder,
					position, coder->previous_byte),
				!is_literal_state(coder->state), match_byte, current_byte);

	make_as_char(coder->optimum[1]);

	uint32_t match_price
			= bit_get_price_1(coder->is_match[coder->state][pos_state]);
	uint32_t rep_match_price
			= match_price + bit_get_price_1(coder->is_rep[coder->state]);


	if (match_byte == current_byte) {
		const uint32_t short_rep_price = rep_match_price
				+ get_rep_len_1_price(coder->state, pos_state);

		if (short_rep_price < coder->optimum[1].price) {
			coder->optimum[1].price = short_rep_price;
			make_as_short_rep(coder->optimum[1]);
		}
	}

	uint32_t len_end = (len_main >= rep_lens[rep_max_index])
			? len_main
			: rep_lens[rep_max_index];

	if (len_end < 2) {
		*back_res = coder->optimum[1].back_prev;
		*len_res = 1;
		return;
	}

	coder->optimum[1].pos_prev = 0;

	for (uint32_t i = 0; i < REP_DISTANCES; ++i)
		coder->optimum[0].backs[i] = reps[i];

	uint32_t len = len_end;
	do {
		coder->optimum[len].price = INFINITY_PRICE;
	} while (--len >= 2);


	uint32_t (*distances_prices)[FULL_DISTANCES] = coder->distances_prices;
	uint32_t (*pos_slot_prices)[DIST_TABLE_SIZE_MAX] = coder->pos_slot_prices;
	uint32_t *align_prices = coder->align_prices;

	for (uint32_t i = 0; i < REP_DISTANCES; ++i) {
		uint32_t rep_len = rep_lens[i];
		if (rep_len < 2)
			continue;

		uint32_t price = rep_match_price;
		get_pure_rep_price(price, i, coder->state, pos_state);

		do {
			const uint32_t cur_and_len_price = price
					+ length_get_price(
					coder->rep_len_encoder,
					rep_len - 2, pos_state);

			if (cur_and_len_price < coder->optimum[rep_len].price) {
				coder->optimum[rep_len].price = cur_and_len_price;
				coder->optimum[rep_len].pos_prev = 0;
				coder->optimum[rep_len].back_prev = i;
				coder->optimum[rep_len].prev_1_is_char = false;
			}
		} while (--rep_len >= 2);
	}


	uint32_t normal_match_price = match_price
			+ bit_get_price_0(coder->is_rep[coder->state]);

	len = (rep_lens[0] >= 2) ? rep_lens[0] + 1 : 2;

	if (len <= len_main) {
		uint32_t offs = 0;

		while (len > match_distances[offs + 1])
			offs += 2;

		for(; ; ++len) {
			const uint32_t distance = match_distances[offs + 2];
			uint32_t cur_and_len_price = normal_match_price;
			get_pos_len_price(cur_and_len_price, distance, len, pos_state);

			if (cur_and_len_price < coder->optimum[len].price) {
				coder->optimum[len].price = cur_and_len_price;
				coder->optimum[len].pos_prev = 0;
				coder->optimum[len].back_prev = distance + REP_DISTANCES;
				coder->optimum[len].prev_1_is_char = false;
			}

			if (len == match_distances[offs + 1]) {
				offs += 2;
				if (offs == num_distance_pairs)
					break;
			}
		}
	}


	//////////////////
	// Big loop ;-) //
	//////////////////

	uint32_t cur = 0;

	// The rest of this function is a huge while-loop. To avoid extreme
	// indentation, the indentation level is not increased here.
	while (true) {

	++cur;

	assert(cur < OPTS);

	if (cur == len_end) {
		backward(coder, len_res, back_res, cur);
		return;
	}

	uint32_t new_len;

	lzma_read_match_distances(coder, &new_len, &num_distance_pairs);

	if (new_len >= fast_bytes) {
		coder->num_distance_pairs = num_distance_pairs;
		coder->longest_match_length = new_len;
		coder->longest_match_was_found = true;
		backward(coder, len_res, back_res, cur);
		return;
	}


	++position;

	uint32_t pos_prev = coder->optimum[cur].pos_prev;
	uint32_t state;

	if (coder->optimum[cur].prev_1_is_char) {
		--pos_prev;

		if (coder->optimum[cur].prev_2) {
			state = coder->optimum[coder->optimum[cur].pos_prev_2].state;

			if (coder->optimum[cur].back_prev_2 < REP_DISTANCES)
				update_long_rep(state);
			else
				update_match(state);

		} else {
			state = coder->optimum[pos_prev].state;
		}

		update_literal(state);

	} else {
		state = coder->optimum[pos_prev].state;
	}

	if (pos_prev == cur - 1) {
		if (is_short_rep(coder->optimum[cur]))
			update_short_rep(state);
		else
			update_literal(state);
	} else {
		uint32_t pos;
		if (coder->optimum[cur].prev_1_is_char && coder->optimum[cur].prev_2) {
			pos_prev = coder->optimum[cur].pos_prev_2;
			pos = coder->optimum[cur].back_prev_2;
			update_long_rep(state);
		} else {
			pos = coder->optimum[cur].back_prev;
			if (pos < REP_DISTANCES)
				update_long_rep(state);
			else
				update_match(state);
		}

		if (pos < REP_DISTANCES) {
			reps[0] = coder->optimum[pos_prev].backs[pos];

			uint32_t i;
			for (i = 1; i <= pos; ++i)
				reps[i] = coder->optimum[pos_prev].backs[i - 1];

			for (; i < REP_DISTANCES; ++i)
				reps[i] = coder->optimum[pos_prev].backs[i];

		} else {
			reps[0] = pos - REP_DISTANCES;

			for (uint32_t i = 1; i < REP_DISTANCES; ++i)
				reps[i] = coder->optimum[pos_prev].backs[i - 1];
		}
	}

	coder->optimum[cur].state = state;

	for (uint32_t i = 0; i < REP_DISTANCES; ++i)
		coder->optimum[cur].backs[i] = reps[i];

	const uint32_t cur_price = coder->optimum[cur].price;

	buf = coder->lz.buffer + coder->lz.read_pos - 1;
	current_byte = *buf;
	match_byte = *(buf - reps[0] - 1);

	pos_state = position & pos_mask;

	const uint32_t cur_and_1_price = cur_price
			+ bit_get_price_0(coder->is_match[state][pos_state])
			+ literal_get_price(
				literal_get_subcoder(coder->literal_coder,
					position, buf[-1]),
        		!is_literal_state(state), match_byte, current_byte);

	bool next_is_char = false;

	if (cur_and_1_price < coder->optimum[cur + 1].price) {
		coder->optimum[cur + 1].price = cur_and_1_price;
		coder->optimum[cur + 1].pos_prev = cur;
		make_as_char(coder->optimum[cur + 1]);
		next_is_char = true;
	}

	match_price = cur_price
			+ bit_get_price_1(coder->is_match[state][pos_state]);
	rep_match_price = match_price
			+ bit_get_price_1(coder->is_rep[state]);

	if (match_byte == current_byte
			&& !(coder->optimum[cur + 1].pos_prev < cur
			&& coder->optimum[cur + 1].back_prev == 0)) {

		const uint32_t short_rep_price = rep_match_price
				+ get_rep_len_1_price(state, pos_state);

		if (short_rep_price <= coder->optimum[cur + 1].price) {
			coder->optimum[cur + 1].price = short_rep_price;
			coder->optimum[cur + 1].pos_prev = cur;
			make_as_short_rep(coder->optimum[cur + 1]);
			next_is_char = true;
		}
	}

	uint32_t num_available_bytes_full
			= coder->lz.write_pos - coder->lz.read_pos + 1;
	num_available_bytes_full = MIN(OPTS - 1 - cur, num_available_bytes_full);
	num_available_bytes = num_available_bytes_full;

	if (num_available_bytes < 2)
		continue;

	if (num_available_bytes > fast_bytes)
		num_available_bytes = fast_bytes;

	if (!next_is_char && match_byte != current_byte) { // speed optimization
		// try literal + rep0
		const uint32_t back_offset = reps[0] + 1;
		const uint32_t limit = MIN(num_available_bytes_full, fast_bytes + 1);

		uint32_t temp;
		for (temp = 1; temp < limit
				&& buf[temp] == *(buf + temp - back_offset);
				++temp) ;

		const uint32_t len_test_2 = temp - 1;

		if (len_test_2 >= 2) {
			uint32_t state_2 = state;
			update_literal(state_2);

			const uint32_t pos_state_next = (position + 1) & pos_mask;
			const uint32_t next_rep_match_price = cur_and_1_price
					+ bit_get_price_1(coder->is_match[state_2][pos_state_next])
					+ bit_get_price_1(coder->is_rep[state_2]);

			// for (; len_test_2 >= 2; --len_test_2) {
			const uint32_t offset = cur + 1 + len_test_2;

			while (len_end < offset)
				coder->optimum[++len_end].price = INFINITY_PRICE;

			uint32_t cur_and_len_price = next_rep_match_price;
			get_rep_price(cur_and_len_price,
					0, len_test_2, state_2, pos_state_next);

			if (cur_and_len_price < coder->optimum[offset].price) {
				coder->optimum[offset].price = cur_and_len_price;
				coder->optimum[offset].pos_prev = cur + 1;
				coder->optimum[offset].back_prev = 0;
				coder->optimum[offset].prev_1_is_char = true;
				coder->optimum[offset].prev_2 = false;
			}
// 			}
		}
	}


	uint32_t start_len = 2; // speed optimization

	for (uint32_t rep_index = 0; rep_index < REP_DISTANCES; ++rep_index) {
		const uint32_t back_offset = reps[rep_index] + 1;

		if (buf[0] != *(buf - back_offset) || buf[1] != *(buf + 1 - back_offset))
			continue;

		uint32_t len_test;
		for (len_test = 2; len_test < num_available_bytes
				&& buf[len_test] == *(buf + len_test - back_offset);
				++len_test) ;

		while (len_end < cur + len_test)
			coder->optimum[++len_end].price = INFINITY_PRICE;

		const uint32_t len_test_temp = len_test;
		uint32_t price = rep_match_price;
		get_pure_rep_price(price, rep_index, state, pos_state);

		do {
			const uint32_t cur_and_len_price = price
					+ length_get_price(coder->rep_len_encoder,
							len_test - 2, pos_state);

			if (cur_and_len_price < coder->optimum[cur + len_test].price) {
				coder->optimum[cur + len_test].price = cur_and_len_price;
				coder->optimum[cur + len_test].pos_prev = cur;
				coder->optimum[cur + len_test].back_prev = rep_index;
				coder->optimum[cur + len_test].prev_1_is_char = false;
			}
		} while (--len_test >= 2);

		len_test = len_test_temp;

		if (rep_index == 0)
			start_len = len_test + 1;


		uint32_t len_test_2 = len_test + 1;
		const uint32_t limit = MIN(num_available_bytes_full,
				len_test_2 + fast_bytes);
		for (; len_test_2 < limit
				&& buf[len_test_2] == *(buf + len_test_2 - back_offset);
				++len_test_2) ;

		len_test_2 -= len_test + 1;

		if (len_test_2 >= 2) {
			uint32_t state_2 = state;
			update_long_rep(state_2);

			uint32_t pos_state_next = (position + len_test) & pos_mask;

			const uint32_t cur_and_len_char_price = price
					+ length_get_price(coder->rep_len_encoder,
						len_test - 2, pos_state)
					+ bit_get_price_0(coder->is_match[state_2][pos_state_next])
					+ literal_get_price(
						literal_get_subcoder(coder->literal_coder,
							position + len_test, buf[len_test - 1]),
						true, *(buf + len_test - back_offset), buf[len_test]);

			update_literal(state_2);

			pos_state_next = (position + len_test + 1) & pos_mask;

			const uint32_t next_rep_match_price = cur_and_len_char_price
					+ bit_get_price_1(coder->is_match[state_2][pos_state_next])
					+ bit_get_price_1(coder->is_rep[state_2]);

// 			for(; len_test_2 >= 2; len_test_2--) {
			const uint32_t offset = cur + len_test + 1 + len_test_2;

			while (len_end < offset)
				coder->optimum[++len_end].price = INFINITY_PRICE;

			uint32_t cur_and_len_price = next_rep_match_price;
			get_rep_price(cur_and_len_price,
					0, len_test_2, state_2, pos_state_next);

			if (cur_and_len_price < coder->optimum[offset].price) {
				coder->optimum[offset].price = cur_and_len_price;
				coder->optimum[offset].pos_prev = cur + len_test + 1;
				coder->optimum[offset].back_prev = 0;
				coder->optimum[offset].prev_1_is_char = true;
				coder->optimum[offset].prev_2 = true;
				coder->optimum[offset].pos_prev_2 = cur;
				coder->optimum[offset].back_prev_2 = rep_index;
			}
// 			}
		}
	}


// 	for (uint32_t len_test = 2; len_test <= new_len; ++len_test)
	if (new_len > num_available_bytes) {
		new_len = num_available_bytes;

		for (num_distance_pairs = 0;
				new_len > match_distances[num_distance_pairs + 1];
				num_distance_pairs += 2) ;

		match_distances[num_distance_pairs + 1] = new_len;
		num_distance_pairs += 2;
	}


	if (new_len >= start_len) {
		normal_match_price = match_price
				+ bit_get_price_0(coder->is_rep[state]);

		while (len_end < cur + new_len)
			coder->optimum[++len_end].price = INFINITY_PRICE;

		uint32_t offs = 0;
		while (start_len > match_distances[offs + 1])
			offs += 2;

		uint32_t cur_back = match_distances[offs + 2];
		uint32_t pos_slot = get_pos_slot_2(cur_back);

		for (uint32_t len_test = start_len; ; ++len_test) {
			uint32_t cur_and_len_price = normal_match_price;
			const uint32_t len_to_pos_state = get_len_to_pos_state(len_test);

			if (cur_back < FULL_DISTANCES)
				cur_and_len_price += distances_prices[
						len_to_pos_state][cur_back];
			else
				cur_and_len_price += pos_slot_prices[
						len_to_pos_state][pos_slot]
						+ align_prices[cur_back & ALIGN_MASK];

			cur_and_len_price += length_get_price(coder->match_len_encoder,
					len_test - MATCH_MIN_LEN, pos_state);

			if (cur_and_len_price < coder->optimum[cur + len_test].price) {
				coder->optimum[cur + len_test].price = cur_and_len_price;
				coder->optimum[cur + len_test].pos_prev = cur;
				coder->optimum[cur + len_test].back_prev
						= cur_back + REP_DISTANCES;
				coder->optimum[cur + len_test].prev_1_is_char = false;
			}

			if (len_test == match_distances[offs + 1]) {
				// Try Match + Literal + Rep0
				const uint32_t back_offset = cur_back + 1;
				uint32_t len_test_2 = len_test + 1;
				const uint32_t limit = MIN(num_available_bytes_full,
						len_test_2 + fast_bytes);

				for (; len_test_2 < limit &&
						buf[len_test_2] == *(buf + len_test_2 - back_offset);
						++len_test_2) ;

				len_test_2 -= len_test + 1;

				if (len_test_2 >= 2) {
					uint32_t state_2 = state;
					update_match(state_2);
					uint32_t pos_state_next
							= (position + len_test) & pos_mask;

					const uint32_t cur_and_len_char_price = cur_and_len_price
							+ bit_get_price_0(
								coder->is_match[state_2][pos_state_next])
							+ literal_get_price(
								literal_get_subcoder(
									coder->literal_coder,
									position + len_test,
									buf[len_test - 1]),
								true,
								*(buf + len_test - back_offset),
								buf[len_test]);

					update_literal(state_2);
					pos_state_next = (pos_state_next + 1) & pos_mask;

					const uint32_t next_rep_match_price
							= cur_and_len_char_price
							+ bit_get_price_1(
								coder->is_match[state_2][pos_state_next])
							+ bit_get_price_1(coder->is_rep[state_2]);

					// for(; len_test_2 >= 2; --len_test_2) {
					const uint32_t offset = cur + len_test + 1 + len_test_2;

					while (len_end < offset)
						coder->optimum[++len_end].price = INFINITY_PRICE;

					cur_and_len_price = next_rep_match_price;
					get_rep_price(cur_and_len_price,
							0, len_test_2, state_2, pos_state_next);

					if (cur_and_len_price < coder->optimum[offset].price) {
						coder->optimum[offset].price = cur_and_len_price;
						coder->optimum[offset].pos_prev = cur + len_test + 1;
						coder->optimum[offset].back_prev = 0;
						coder->optimum[offset].prev_1_is_char = true;
						coder->optimum[offset].prev_2 = true;
						coder->optimum[offset].pos_prev_2 = cur;
						coder->optimum[offset].back_prev_2
								= cur_back + REP_DISTANCES;
					}
// 					}
				}

				offs += 2;
				if (offs == num_distance_pairs)
					break;

				cur_back = match_distances[offs + 2];
				if (cur_back >= FULL_DISTANCES)
					pos_slot = get_pos_slot_2(cur_back);
			}
		}
	}

	} // Closes: while (true)
}
