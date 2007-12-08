///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_getoptimumfast.c
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


#define change_pair(small_dist, big_dist) \
	(((big_dist) >> 7) > (small_dist))


extern void
lzma_get_optimum_fast(lzma_coder *restrict coder,
		uint32_t *restrict back_res, uint32_t *restrict len_res)
{
	// Local copies
	const uint32_t fast_bytes = coder->fast_bytes;

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
		// There's not enough input left to encode a match.
		*back_res = UINT32_MAX;
		*len_res = 1;
		return;
	}

	if (num_available_bytes > MATCH_MAX_LEN)
		num_available_bytes = MATCH_MAX_LEN;


	// Look for repetitive matches; scan the previous four match distances
	uint32_t rep_lens[REP_DISTANCES];
	uint32_t rep_max_index = 0;

	for (uint32_t i = 0; i < REP_DISTANCES; ++i) {
		const uint32_t back_offset = coder->rep_distances[i] + 1;

		// If the first two bytes (2 == MATCH_MIN_LEN) do not match,
		// this rep_distance[i] is not useful. This is indicated
		// using zero as the length of the repetitive match.
		if (buf[0] != *(buf - back_offset)
				|| buf[1] != *(buf + 1 - back_offset)) {
			rep_lens[i] = 0;
			continue;
		}

		// The first two bytes matched.
		// Calculate the length of the match.
		uint32_t len;
		for (len = 2; len < num_available_bytes
				&& buf[len] == *(buf + len - back_offset);
				++len) ;

		// If we have found a repetitive match that is at least
		// as long as fast_bytes, return it immediatelly.
		if (len >= fast_bytes) {
			*back_res = i;
			*len_res = len;
			move_pos(len - 1);
			return;
		}

		rep_lens[i] = len;

		// After this loop, rep_lens[rep_max_index] is the biggest
		// value of all values in rep_lens[].
		if (len > rep_lens[rep_max_index])
			rep_max_index = i;
	}


	if (len_main >= fast_bytes) {
		*back_res = coder->match_distances[num_distance_pairs]
				+ REP_DISTANCES;
		*len_res = len_main;
		move_pos(len_main - 1);
		return;
	}

	uint32_t back_main = 0;
	if (len_main >= 2) {
		back_main = coder->match_distances[num_distance_pairs];

		while (num_distance_pairs > 2 && len_main ==
				coder->match_distances[num_distance_pairs - 3] + 1) {
			if (!change_pair(coder->match_distances[
					num_distance_pairs - 2], back_main))
				break;

			num_distance_pairs -= 2;
			len_main = coder->match_distances[num_distance_pairs - 1];
			back_main = coder->match_distances[num_distance_pairs];
		}

		if (len_main == 2 && back_main >= 0x80)
			len_main = 1;
	}

	if (rep_lens[rep_max_index] >= 2) {
		if (rep_lens[rep_max_index] + 1 >= len_main
				|| (rep_lens[rep_max_index] + 2 >= len_main
					&& (back_main > (1 << 9)))
				|| (rep_lens[rep_max_index] + 3 >= len_main
					&& (back_main > (1 << 15)))) {
			*back_res = rep_max_index;
			*len_res = rep_lens[rep_max_index];
			move_pos(*len_res - 1);
			return;
		}
	}

	if (len_main >= 2 && num_available_bytes > 2) {
		lzma_read_match_distances(coder, &coder->longest_match_length,
				&coder->num_distance_pairs);

		if (coder->longest_match_length >= 2) {
			const uint32_t new_distance = coder->match_distances[
					coder->num_distance_pairs];

			if ((coder->longest_match_length >= len_main
						&& new_distance < back_main)
					|| (coder->longest_match_length == len_main + 1
						&& !change_pair(back_main, new_distance))
					|| (coder->longest_match_length > len_main + 1)
					|| (coder->longest_match_length + 1 >= len_main
						&& len_main >= 3
						&& change_pair(new_distance, back_main))) {
				coder->longest_match_was_found = true;
				*back_res = UINT32_MAX;
				*len_res = 1;
				return;
			}
		}

		++buf;
		--num_available_bytes;

		for (uint32_t i = 0; i < REP_DISTANCES; ++i) {
			const uint32_t back_offset = coder->rep_distances[i] + 1;

			if (buf[1] != *(buf + 1 - back_offset)
					|| buf[2] != *(buf + 2 - back_offset)) {
				rep_lens[i] = 0;
				continue;
			}

			uint32_t len;
			for (len = 2; len < num_available_bytes
					&& buf[len] == *(buf + len - back_offset);
					++len) ;

			if (len + 1 >= len_main) {
				coder->longest_match_was_found = true;
				*back_res = UINT32_MAX;
				*len_res = 1;
				return;
			}
		}

		*back_res = back_main + REP_DISTANCES;
		*len_res = len_main;
		move_pos(len_main - 2);
		return;
	}

	*back_res = UINT32_MAX;
	*len_res = 1;
	return;
}
