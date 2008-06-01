///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder.c
/// \brief      LZ in window
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

#include "lz_encoder_private.h"

// Hash Chains
#ifdef HAVE_HC3
#	include "hc3.h"
#endif
#ifdef HAVE_HC4
#	include "hc4.h"
#endif

// Binary Trees
#ifdef HAVE_BT2
#	include "bt2.h"
#endif
#ifdef HAVE_BT3
#	include "bt3.h"
#endif
#ifdef HAVE_BT4
#	include "bt4.h"
#endif


/// This is needed in two places so provide a macro.
#define get_cyclic_buffer_size(history_size) ((history_size) + 1)


/// Calculate certain match finder properties and validate the calculated
/// values. This is as its own function, because *num_items is needed to
/// calculate memory requirements in common/memory.c.
extern bool
lzma_lz_encoder_hash_properties(lzma_match_finder match_finder,
		uint32_t history_size, uint32_t *restrict hash_mask,
		uint32_t *restrict hash_size_sum, uint32_t *restrict num_items)
{
	uint32_t fix_hash_size;
	uint32_t sons;

	switch (match_finder) {
#ifdef HAVE_HC3
	case LZMA_MF_HC3:
		fix_hash_size = LZMA_HC3_FIX_HASH_SIZE;
		sons = 1;
		break;
#endif
#ifdef HAVE_HC4
	case LZMA_MF_HC4:
		fix_hash_size = LZMA_HC4_FIX_HASH_SIZE;
		sons = 1;
		break;
#endif
#ifdef HAVE_BT2
	case LZMA_MF_BT2:
		fix_hash_size = LZMA_BT2_FIX_HASH_SIZE;
		sons = 2;
		break;
#endif
#ifdef HAVE_BT3
	case LZMA_MF_BT3:
		fix_hash_size = LZMA_BT3_FIX_HASH_SIZE;
		sons = 2;
		break;
#endif
#ifdef HAVE_BT4
	case LZMA_MF_BT4:
		fix_hash_size = LZMA_BT4_FIX_HASH_SIZE;
		sons = 2;
		break;
#endif
	default:
		return true;
	}

	uint32_t hs;

#ifdef HAVE_LZMA_BT2
	if (match_finder == LZMA_BT2) {
		// NOTE: hash_mask is not used by the BT2 match finder,
		// but it is initialized just in case.
		hs = LZMA_BT2_HASH_SIZE;
		*hash_mask = 0;
	} else
#endif
	{
		hs = history_size - 1;
		hs |= (hs >> 1);
		hs |= (hs >> 2);
		hs |= (hs >> 4);
		hs |= (hs >> 8);
		hs >>= 1;
		hs |= 0xFFFF;

		if (hs > (UINT32_C(1) << 24)) {
			if (match_finder == LZMA_MF_HC4
					|| match_finder == LZMA_MF_BT4)
				hs >>= 1;
			else
				hs = (1 << 24) - 1;
		}

		*hash_mask = hs;
		++hs;
	}

	*hash_size_sum = hs + fix_hash_size;

	*num_items = *hash_size_sum
			+ get_cyclic_buffer_size(history_size) * sons;

	return false;
}


extern lzma_ret
lzma_lz_encoder_reset(lzma_lz_encoder *lz, lzma_allocator *allocator,
		bool (*process)(lzma_coder *coder, uint8_t *restrict out,
			size_t *restrict out_pos, size_t out_size),
		size_t history_size, size_t additional_buffer_before,
		size_t match_max_len, size_t additional_buffer_after,
		lzma_match_finder match_finder, uint32_t match_finder_cycles,
		const uint8_t *preset_dictionary,
		size_t preset_dictionary_size)
{
	lz->sequence = SEQ_RUN;

	///////////////
	// In Window //
	///////////////

	// Validate history size.
	if (history_size < LZMA_DICTIONARY_SIZE_MIN
			|| history_size > LZMA_DICTIONARY_SIZE_MAX) {
		lzma_lz_encoder_end(lz, allocator);
		return LZMA_HEADER_ERROR;
	}

	assert(history_size <= MAX_VAL_FOR_NORMALIZE - 256);
	assert(LZMA_DICTIONARY_SIZE_MAX <= MAX_VAL_FOR_NORMALIZE - 256);

	// Calculate the size of the history buffer to allocate.
	// TODO: Get a reason for magic constant of 256.
	const size_t size_reserv = (history_size + additional_buffer_before
			+ match_max_len + additional_buffer_after) / 2 + 256;

	lz->keep_size_before = history_size + additional_buffer_before;
	lz->keep_size_after = match_max_len + additional_buffer_after;

	const size_t buffer_size = lz->keep_size_before + lz->keep_size_after
			+ size_reserv;

	// Allocate history buffer if its size has changed.
	if (buffer_size != lz->size) {
		lzma_free(lz->buffer, allocator);
		lz->buffer = lzma_alloc(buffer_size, allocator);
		if (lz->buffer == NULL) {
			lzma_lz_encoder_end(lz, allocator);
			return LZMA_MEM_ERROR;
		}
	}

	// Allocation successful. Store the new size.
	lz->size = buffer_size;

	// Reset in window variables.
	lz->offset = 0;
	lz->read_pos = 0;
	lz->read_limit = 0;
	lz->write_pos = 0;
	lz->pending = 0;


	//////////////////
	// Match Finder //
	//////////////////

	// Validate match_finder, set function pointers and a few match
	// finder specific variables.
	switch (match_finder) {
#ifdef HAVE_HC3
	case LZMA_MF_HC3:
		lz->get_matches = &lzma_hc3_get_matches;
		lz->skip = &lzma_hc3_skip;
		lz->cut_value = 8 + (match_max_len >> 2);
		break;
#endif
#ifdef HAVE_HC4
	case LZMA_MF_HC4:
		lz->get_matches = &lzma_hc4_get_matches;
		lz->skip = &lzma_hc4_skip;
		lz->cut_value = 8 + (match_max_len >> 2);
		break;
#endif
#ifdef HAVE_BT2
	case LZMA_MF_BT2:
		lz->get_matches = &lzma_bt2_get_matches;
		lz->skip = &lzma_bt2_skip;
		lz->cut_value = 16 + (match_max_len >> 1);
		break;
#endif
#ifdef HAVE_BT3
	case LZMA_MF_BT3:
		lz->get_matches = &lzma_bt3_get_matches;
		lz->skip = &lzma_bt3_skip;
		lz->cut_value = 16 + (match_max_len >> 1);
		break;
#endif
#ifdef HAVE_BT4
	case LZMA_MF_BT4:
		lz->get_matches = &lzma_bt4_get_matches;
		lz->skip = &lzma_bt4_skip;
		lz->cut_value = 16 + (match_max_len >> 1);
		break;
#endif
	default:
		lzma_lz_encoder_end(lz, allocator);
		return LZMA_HEADER_ERROR;
	}

	// Check if we have been requested to use a non-default cut_value.
	if (match_finder_cycles > 0)
		lz->cut_value = match_finder_cycles;

	lz->match_max_len = match_max_len;
	lz->cyclic_buffer_size = get_cyclic_buffer_size(history_size);

	uint32_t hash_size_sum;
	uint32_t num_items;
	if (lzma_lz_encoder_hash_properties(match_finder, history_size,
			&lz->hash_mask, &hash_size_sum, &num_items)) {
		lzma_lz_encoder_end(lz, allocator);
		return LZMA_HEADER_ERROR;
	}

	if (num_items != lz->num_items) {
#if UINT32_MAX >= SIZE_MAX / 4
		// Check for integer overflow. (Huge dictionaries are not
		// possible on 32-bit CPU.)
		if (num_items > SIZE_MAX / sizeof(uint32_t)) {
			lzma_lz_encoder_end(lz, allocator);
			return LZMA_MEM_ERROR;
		}
#endif

		const size_t size_in_bytes
				= (size_t)(num_items) * sizeof(uint32_t);

		lzma_free(lz->hash, allocator);
		lz->hash = lzma_alloc(size_in_bytes, allocator);
		if (lz->hash == NULL) {
			lzma_lz_encoder_end(lz, allocator);
			return LZMA_MEM_ERROR;
		}

		lz->num_items = num_items;
	}

	lz->son = lz->hash + hash_size_sum;

	// Reset the hash table to empty hash values.
	{
		uint32_t *restrict items = lz->hash;

		for (uint32_t i = 0; i < hash_size_sum; ++i)
			items[i] = EMPTY_HASH_VALUE;
	}

	lz->cyclic_buffer_pos = 0;

	// Because zero is used as empty hash value, make the first byte
	// appear at buffer[1 - offset].
	++lz->offset;

	// If we are using a preset dictionary, read it now.
	// TODO: This isn't implemented yet so return LZMA_HEADER_ERROR.
	if (preset_dictionary != NULL && preset_dictionary_size > 0) {
		lzma_lz_encoder_end(lz, allocator);
		return LZMA_HEADER_ERROR;
	}

	// Set the process function pointer.
	lz->process = process;

	return LZMA_OK;
}


extern void
lzma_lz_encoder_end(lzma_lz_encoder *lz, lzma_allocator *allocator)
{
	lzma_free(lz->hash, allocator);
	lz->hash = NULL;
	lz->num_items = 0;

	lzma_free(lz->buffer, allocator);
	lz->buffer = NULL;
	lz->size = 0;

	return;
}


/// \brief      Moves the data in the input window to free space for new data
///
/// lz->buffer is a sliding input window, which keeps lz->keep_size_before
/// bytes of input history available all the time. Now and then we need to
/// "slide" the buffer to make space for the new data to the end of the
/// buffer. At the same time, data older than keep_size_before is dropped.
///
static void
move_window(lzma_lz_encoder *lz)
{
	// buffer[move_offset] will become buffer[0].
	assert(lz->read_pos > lz->keep_size_after);
	size_t move_offset = lz->read_pos - lz->keep_size_before;

	// We need one additional byte, since move_pos() moves on 1 byte.
	// TODO: Clean up? At least document more.
	if (move_offset > 0)
		--move_offset;

	assert(lz->write_pos > move_offset);
	const size_t move_size = lz->write_pos - move_offset;

	assert(move_offset + move_size <= lz->size);

	memmove(lz->buffer, lz->buffer + move_offset, move_size);

	lz->offset += move_offset;
	lz->read_pos -= move_offset;
	lz->read_limit -= move_offset;
	lz->write_pos -= move_offset;

	return;
}


/// \brief      Tries to fill the input window (lz->buffer)
///
/// If we are the last encoder in the chain, our input data is in in[].
/// Otherwise we call the next filter in the chain to process in[] and
/// write its output to lz->buffer.
///
/// This function must not be called once it has returned LZMA_STREAM_END.
///
static lzma_ret
fill_window(lzma_coder *coder, lzma_allocator *allocator, const uint8_t *in,
		size_t *in_pos, size_t in_size, lzma_action action)
{
	assert(coder->lz.read_pos <= coder->lz.write_pos);

	// Move the sliding window if needed.
	if (coder->lz.read_pos >= coder->lz.size - coder->lz.keep_size_after)
		move_window(&coder->lz);

	size_t in_used;
	lzma_ret ret;
	if (coder->next.code == NULL) {
		// Not using a filter, simply memcpy() as much as possible.
		in_used = bufcpy(in, in_pos, in_size, coder->lz.buffer,
				&coder->lz.write_pos, coder->lz.size);

		if (action != LZMA_RUN && *in_pos == in_size)
			ret = LZMA_STREAM_END;
		else
			ret = LZMA_OK;

	} else {
		const size_t in_start = *in_pos;
		ret = coder->next.code(coder->next.coder, allocator,
				in, in_pos, in_size,
				coder->lz.buffer, &coder->lz.write_pos,
				coder->lz.size, action);
		in_used = *in_pos - in_start;
	}

	// If end of stream has been reached or flushing completed, we allow
	// the encoder to process all the input (that is, read_pos is allowed
	// to reach write_pos). Otherwise we keep keep_size_after bytes
	// available as prebuffer.
	if (ret == LZMA_STREAM_END) {
		assert(*in_pos == in_size);
		coder->lz.read_limit = coder->lz.write_pos;
		ret = LZMA_OK;

		switch (action) {
		case LZMA_SYNC_FLUSH:
			coder->lz.sequence = SEQ_FLUSH;
			break;

		case LZMA_FINISH:
			coder->lz.sequence = SEQ_FINISH;
			break;

		default:
			assert(0);
			ret = LZMA_PROG_ERROR;
			break;
		}

	} else if (coder->lz.write_pos > coder->lz.keep_size_after) {
		// This needs to be done conditionally, because if we got
		// only little new input, there may be too little input
		// to do any encoding yet.
		coder->lz.read_limit = coder->lz.write_pos
				- coder->lz.keep_size_after;
	}

	// Restart the match finder after finished LZMA_SYNC_FLUSH.
	if (coder->lz.pending > 0
			&& coder->lz.read_pos < coder->lz.read_limit) {
		// Match finder may update coder->pending and expects it to
		// start from zero, so use a temporary variable.
		const size_t pending = coder->lz.pending;
		coder->lz.pending = 0;

		// Rewind read_pos so that the match finder can hash
		// the pending bytes.
		assert(coder->lz.read_pos >= pending);
		coder->lz.read_pos -= pending;
		coder->lz.skip(&coder->lz, pending);
	}

	return ret;
}


extern lzma_ret
lzma_lz_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size,
		uint8_t *restrict out, size_t *restrict out_pos,
		size_t out_size, lzma_action action)
{
	while (*out_pos < out_size
			&& (*in_pos < in_size || action != LZMA_RUN)) {
		// Read more data to coder->lz.buffer if needed.
		if (coder->lz.sequence == SEQ_RUN
				&& coder->lz.read_pos >= coder->lz.read_limit)
			return_if_error(fill_window(coder, allocator,
					in, in_pos, in_size, action));

		// Encode
		if (coder->lz.process(coder, out, out_pos, out_size)) {
			// Setting this to SEQ_RUN for cases when we are
			// flushing. It doesn't matter when finishing.
			coder->lz.sequence = SEQ_RUN;
			return action != LZMA_RUN ? LZMA_STREAM_END : LZMA_OK;
		}
	}

	return LZMA_OK;
}


/// \brief      Normalizes hash values
///
/// lzma_lz_normalize is called when lz->pos hits MAX_VAL_FOR_NORMALIZE,
/// which currently happens once every 2 GiB of input data (to be exact,
/// after the first 2 GiB it happens once every 2 GiB minus dictionary_size
/// bytes). lz->pos is incremented by lzma_lz_move_pos().
///
/// lz->hash contains big amount of offsets relative to lz->buffer.
/// The offsets are stored as uint32_t, which is the only reasonable
/// datatype for these offsets; uint64_t would waste far too much RAM
/// and uint16_t would limit the dictionary to 64 KiB (far too small).
///
/// When compressing files over 2 GiB, lz->buffer needs to be moved forward
/// to avoid integer overflows. We scan the lz->hash array and fix every
/// value to match the updated lz->buffer.
extern void
lzma_lz_encoder_normalize(lzma_lz_encoder *lz)
{
	const uint32_t subvalue = lz->read_pos - lz->cyclic_buffer_size;
	assert(subvalue <= INT32_MAX);

	{
		const uint32_t num_items = lz->num_items;
		uint32_t *restrict items = lz->hash;

		for (uint32_t i = 0; i < num_items; ++i) {
			// If the distance is greater than the dictionary
			// size, we can simply mark the item as empty.
			if (items[i] <= subvalue)
				items[i] = EMPTY_HASH_VALUE;
			else
				items[i] -= subvalue;
		}
	}

	// Update offset to match the new locations.
	lz->offset -= subvalue;

	return;
}
