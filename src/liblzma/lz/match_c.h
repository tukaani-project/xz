///////////////////////////////////////////////////////////////////////////////
//
/// \file       match_c.h
/// \brief      Template for different match finders
///
/// This file is included by hc3.c, hc4, bt2.c, bt3.c and bt4.c. Each file
/// sets slighly different #defines, resulting the different match finders.
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

//////////////
// Includes //
//////////////

#include "check.h"


///////////////
// Constants //
///////////////

#define START_MAX_LEN 1

#ifdef HASH_ARRAY_2
#	define NUM_HASH_DIRECT_BYTES 0
#	define HASH_2_SIZE (1 << 10)
#	ifdef HASH_ARRAY_3
#		define NUM_HASH_BYTES 4
#		define HASH_3_SIZE (1 << 16)
#		define HASH_3_OFFSET HASH_2_SIZE
#		define FIX_HASH_SIZE (HASH_2_SIZE + HASH_3_SIZE)
#	else
#		define NUM_HASH_BYTES 3
#		define FIX_HASH_SIZE HASH_2_SIZE
#	endif
#	define HASH_SIZE 0
#	define MIN_MATCH_CHECK NUM_HASH_BYTES
#else
#	define NUM_HASH_DIRECT_BYTES 2
#	define NUM_HASH_BYTES 2
#	define HASH_SIZE (1 << (8 * NUM_HASH_BYTES))
#	define MIN_MATCH_CHECK (NUM_HASH_BYTES + 1)
#	define FIX_HASH_SIZE 0
#endif


////////////
// Macros //
////////////

#ifdef HASH_ARRAY_2
#	ifdef HASH_ARRAY_3
#		define HASH_CALC() \
		do { \
			const uint32_t temp = lzma_crc32_table[0][ \
					cur[0]] ^ cur[1]; \
			hash_2_value = temp & (HASH_2_SIZE - 1); \
			hash_3_value = (temp ^ ((uint32_t)(cur[2]) << 8)) \
					& (HASH_3_SIZE - 1); \
			hash_value = (temp ^ ((uint32_t)(cur[2]) << 8) \
					^ (lzma_crc32_table[0][cur[3]] << 5)) \
					& lz->hash_mask; \
		} while (0)
#	else
#		define HASH_CALC() \
		do { \
			const uint32_t temp = lzma_crc32_table[0][ \
					cur[0]] ^ cur[1]; \
			hash_2_value = temp & (HASH_2_SIZE - 1); \
			hash_value = (temp ^ ((uint32_t)(cur[2]) << 8)) \
					& lz->hash_mask; \
		} while (0)
#	endif
#else
#	define HASH_CALC() hash_value = cur[0] ^ ((uint32_t)(cur[1]) << 8)
#endif


// Moves the current read position forward by one byte. In LZMA SDK,
// CLZInWindow::MovePos() can read more input data if needed, because of
// the callback style API. In liblzma we must have ensured earlier, that
// there is enough data available in lz->buffer.
#define move_pos() \
do { \
	if (++lz->cyclic_buffer_pos == lz->cyclic_buffer_size) \
		lz->cyclic_buffer_pos = 0; \
	++lz->read_pos; \
	assert(lz->read_pos <= lz->write_pos); \
	if (lz->read_pos == MAX_VAL_FOR_NORMALIZE) \
		lzma_lz_encoder_normalize(lz); \
} while (0)


//////////////////////
// Global constants //
//////////////////////

LZMA_HASH_SIZE(LZMA_MATCH_FINDER_NAME_UPPER) = HASH_SIZE;
LZMA_FIX_HASH_SIZE(LZMA_MATCH_FINDER_NAME_UPPER) = FIX_HASH_SIZE;


///////////////////
// API functions //
///////////////////

LZMA_GET_MATCHES(LZMA_MATCH_FINDER_NAME_LOWER)
{
	uint32_t len_limit;
	if (lz->read_pos + lz->match_max_len <= lz->write_pos) {
		len_limit = lz->match_max_len;
	} else {
		assert(lz->stream_end_was_reached);
		len_limit = lz->write_pos - lz->read_pos;
		if (len_limit < MIN_MATCH_CHECK) {
			distances[0] = 0;
			move_pos();
			return;
		}
	}

	int32_t offset = 1;
	const uint32_t match_min_pos
			= lz->read_pos + lz->offset > lz->cyclic_buffer_size
			? lz->read_pos + lz->offset - lz->cyclic_buffer_size
			: 0;
	const uint8_t *cur = lz->buffer + lz->read_pos;
	uint32_t max_len = START_MAX_LEN; // to avoid items for len < hash_size

#ifdef HASH_ARRAY_2
	uint32_t hash_2_value;
#	ifdef HASH_ARRAY_3
	uint32_t hash_3_value;
#	endif
#endif
	uint32_t hash_value;
	HASH_CALC();

	uint32_t cur_match = lz->hash[FIX_HASH_SIZE + hash_value];
#ifdef HASH_ARRAY_2
	uint32_t cur_match2 = lz->hash[hash_2_value];
#	ifdef HASH_ARRAY_3
	uint32_t cur_match3 = lz->hash[HASH_3_OFFSET + hash_3_value];
#	endif
	lz->hash[hash_2_value] = lz->read_pos + lz->offset;

	if (cur_match2 > match_min_pos) {
		if (lz->buffer[cur_match2 - lz->offset] == cur[0]) {
			max_len = 2;
			distances[offset++] = 2;
			distances[offset++] = lz->read_pos + lz->offset
					- cur_match2 - 1;
		}
	}

#	ifdef HASH_ARRAY_3
	lz->hash[HASH_3_OFFSET + hash_3_value] = lz->read_pos + lz->offset;
	if (cur_match3 > match_min_pos) {
		if (lz->buffer[cur_match3 - lz->offset] == cur[0]) {
			if (cur_match3 == cur_match2)
				offset -= 2;

			max_len = 3;
			distances[offset++] = 3;
			distances[offset++] = lz->read_pos + lz->offset
					- cur_match3 - 1;
			cur_match2 = cur_match3;
		}
	}
#	endif

	if (offset != 1 && cur_match2 == cur_match) {
		offset -= 2;
		max_len = START_MAX_LEN;
	}
#endif

	lz->hash[FIX_HASH_SIZE + hash_value] = lz->read_pos + lz->offset;

#ifdef IS_HASH_CHAIN
	lz->son[lz->cyclic_buffer_pos] = cur_match;
#else
	uint32_t *ptr0 = lz->son + (lz->cyclic_buffer_pos << 1) + 1;
	uint32_t *ptr1 = lz->son + (lz->cyclic_buffer_pos << 1);

	uint32_t len0 = NUM_HASH_DIRECT_BYTES;
	uint32_t len1 = NUM_HASH_DIRECT_BYTES;
#endif

#if NUM_HASH_DIRECT_BYTES != 0
	if (cur_match > match_min_pos) {
		if (lz->buffer[cur_match + NUM_HASH_DIRECT_BYTES - lz->offset]
				!= cur[NUM_HASH_DIRECT_BYTES]) {
			max_len = NUM_HASH_DIRECT_BYTES;
			distances[offset++] = NUM_HASH_DIRECT_BYTES;
			distances[offset++] = lz->read_pos + lz->offset
					- cur_match - 1;
		}
	}
#endif

	uint32_t count = lz->cut_value;

	while (true) {
		if (cur_match <= match_min_pos || count-- == 0) {
#ifndef IS_HASH_CHAIN
			*ptr0 = EMPTY_HASH_VALUE;
			*ptr1 = EMPTY_HASH_VALUE;
#endif
			break;
		}

		const uint32_t delta = lz->read_pos + lz->offset - cur_match;
		const uint32_t cyclic_pos = delta <= lz->cyclic_buffer_pos
				? lz->cyclic_buffer_pos - delta
				: lz->cyclic_buffer_pos - delta
					+ lz->cyclic_buffer_size;
		uint32_t *pair = lz->son +
#ifdef IS_HASH_CHAIN
				cyclic_pos;
#else
				(cyclic_pos << 1);
#endif

		const uint8_t *pb = lz->buffer + cur_match - lz->offset;
		uint32_t len =
#ifdef IS_HASH_CHAIN
				NUM_HASH_DIRECT_BYTES;
		if (pb[max_len] == cur[max_len])
#else
				MIN(len0, len1);
#endif

		if (pb[len] == cur[len]) {
			while (++len != len_limit)
				if (pb[len] != cur[len])
					break;

			if (max_len < len) {
				max_len = len;
				distances[offset++] = len;
				distances[offset++] = delta - 1;
				if (len == len_limit) {
#ifndef IS_HASH_CHAIN
					*ptr1 = pair[0];
					*ptr0 = pair[1];
#endif
					break;
				}
			}
		}

#ifdef IS_HASH_CHAIN
		cur_match = *pair;
#else
		if (pb[len] < cur[len]) {
			*ptr1 = cur_match;
			ptr1 = pair + 1;
			cur_match = *ptr1;
			len1 = len;
		} else {
			*ptr0 = cur_match;
			ptr0 = pair;
			cur_match = *ptr0;
			len0 = len;
		}
#endif
	}

	distances[0] = offset - 1;

	move_pos();

	return;
}


LZMA_SKIP(LZMA_MATCH_FINDER_NAME_LOWER)
{
	do {
#ifdef IS_HASH_CHAIN
		if (lz->write_pos - lz->read_pos < NUM_HASH_BYTES) {
			move_pos();
			continue;
		}
#else
		uint32_t len_limit;
		if (lz->read_pos + lz->match_max_len <= lz->write_pos) {
			len_limit = lz->match_max_len;
		} else {
			assert(lz->stream_end_was_reached == true);
			len_limit = lz->write_pos - lz->read_pos;
			if (len_limit < MIN_MATCH_CHECK) {
				move_pos();
				continue;
			}
		}
		const uint32_t match_min_pos
			= lz->read_pos + lz->offset > lz->cyclic_buffer_size
			? lz->read_pos + lz->offset - lz->cyclic_buffer_size
			: 0;
#endif

		const uint8_t *cur = lz->buffer + lz->read_pos;

#ifdef HASH_ARRAY_2
		uint32_t hash_2_value;
#	ifdef HASH_ARRAY_3
		uint32_t hash_3_value;
		uint32_t hash_value;
		HASH_CALC();
		lz->hash[HASH_3_OFFSET + hash_3_value]
				= lz->read_pos + lz->offset;
#	else
		uint32_t hash_value;
		HASH_CALC();
#	endif
		lz->hash[hash_2_value] = lz->read_pos + lz->offset;
#else
		uint32_t hash_value;
		HASH_CALC();
#endif

		uint32_t cur_match = lz->hash[FIX_HASH_SIZE + hash_value];
		lz->hash[FIX_HASH_SIZE + hash_value]
				= lz->read_pos + lz->offset;

#ifdef IS_HASH_CHAIN
		lz->son[lz->cyclic_buffer_pos] = cur_match;
#else
		uint32_t *ptr0 = lz->son + (lz->cyclic_buffer_pos << 1) + 1;
		uint32_t *ptr1 = lz->son + (lz->cyclic_buffer_pos << 1);

		uint32_t len0 = NUM_HASH_DIRECT_BYTES;
		uint32_t len1 = NUM_HASH_DIRECT_BYTES;
		uint32_t count = lz->cut_value;

		while (true) {
			if (cur_match <= match_min_pos || count-- == 0) {
				*ptr0 = EMPTY_HASH_VALUE;
				*ptr1 = EMPTY_HASH_VALUE;
				break;
			}

			const uint32_t delta = lz->read_pos
					+ lz->offset - cur_match;
			const uint32_t cyclic_pos
					= delta <= lz->cyclic_buffer_pos
					? lz->cyclic_buffer_pos - delta
					: lz->cyclic_buffer_pos - delta
						+ lz->cyclic_buffer_size;
			uint32_t *pair = lz->son + (cyclic_pos << 1);

			const uint8_t *pb = lz->buffer + cur_match
					- lz->offset;
			uint32_t len = MIN(len0, len1);

			if (pb[len] == cur[len]) {
				while (++len != len_limit)
					if (pb[len] != cur[len])
						break;

				if (len == len_limit) {
					*ptr1 = pair[0];
					*ptr0 = pair[1];
					break;
				}
			}

			if (pb[len] < cur[len]) {
				*ptr1 = cur_match;
				ptr1 = pair + 1;
				cur_match = *ptr1;
				len1 = len;
			} else {
				*ptr0 = cur_match;
				ptr0 = pair;
				cur_match = *ptr0;
				len0 = len;
			}
		}
#endif

		move_pos();

	} while (--num != 0);

	return;
}
