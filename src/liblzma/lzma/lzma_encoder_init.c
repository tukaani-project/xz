///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_init.c
/// \brief      Creating, resetting and destroying the LZMA encoder
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

#include "lzma_encoder_private.h"


/// \brief      Initializes the length encoder
static void
length_encoder_reset(lzma_length_encoder *lencoder,
		const uint32_t num_pos_states, const uint32_t table_size)
{
	// NLength::CPriceTableEncoder::SetTableSize()
	lencoder->table_size = table_size;

	// NLength::CEncoder::Init()
	bit_reset(lencoder->choice);
	bit_reset(lencoder->choice2);

	for (size_t pos_state = 0; pos_state < num_pos_states; ++pos_state) {
		bittree_reset(lencoder->low[pos_state], LEN_LOW_BITS);
		bittree_reset(lencoder->mid[pos_state], LEN_MID_BITS);
	}

	bittree_reset(lencoder->high, LEN_HIGH_BITS);

	// NLength::CPriceTableEncoder::UpdateTables()
	for (size_t pos_state = 0; pos_state < num_pos_states; ++pos_state)
		lzma_length_encoder_update_table(lencoder, pos_state);

	return;
}


static void
lzma_lzma_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_lz_encoder_end(&coder->lz, allocator);
	lzma_literal_end(&coder->literal_coder, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_lzma_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->lz = LZMA_LZ_ENCODER_INIT;
		next->coder->literal_coder = NULL;
	}

	// Validate options that aren't validated elsewhere.
	const lzma_options_lzma *options = filters[0].options;
	if (options->pos_bits > LZMA_POS_BITS_MAX
			|| options->fast_bytes < LZMA_FAST_BYTES_MIN
			|| options->fast_bytes > LZMA_FAST_BYTES_MAX) {
		lzma_lzma_encoder_end(next->coder, allocator);
		return LZMA_HEADER_ERROR;
	}

	// Set compression mode.
	switch (options->mode) {
		case LZMA_MODE_FAST:
			next->coder->best_compression = false;
			break;

		case LZMA_MODE_BEST:
			next->coder->best_compression = true;
			break;

		default:
			lzma_lzma_encoder_end(next->coder, allocator);
			return LZMA_HEADER_ERROR;
	}

	// Initialize literal coder.
	{
		const lzma_ret ret = lzma_literal_init(
				&next->coder->literal_coder, allocator,
				options->literal_context_bits,
				options->literal_pos_bits);
		if (ret != LZMA_OK) {
			lzma_lzma_encoder_end(next->coder, allocator);
			return ret;
		}
	}

	// Initialize LZ encoder.
	{
		const lzma_ret ret = lzma_lz_encoder_reset(
				&next->coder->lz, allocator, &lzma_lzma_encode,
				filters[0].uncompressed_size,
				options->dictionary_size, OPTS,
				options->fast_bytes, MATCH_MAX_LEN + 1 + OPTS,
				options->match_finder,
				options->match_finder_cycles,
				options->preset_dictionary,
				options->preset_dictionary_size);
		if (ret != LZMA_OK) {
			lzma_lzma_encoder_end(next->coder, allocator);
			return ret;
		}
	}

	// Set dist_table_size.
	{
		// Round the dictionary size up to next 2^n.
		uint32_t log_size;
		for (log_size = 0; (UINT32_C(1) << log_size)
				< options->dictionary_size; ++log_size) ;

		next->coder->dist_table_size = log_size * 2;
	}

	// Misc FIXME desc
	next->coder->align_price_count = 0;
	next->coder->match_price_count = 0;
	next->coder->dictionary_size = options->dictionary_size;
	next->coder->pos_mask = (1U << options->pos_bits) - 1;
	next->coder->fast_bytes = options->fast_bytes;

	// Range coder
	rc_reset(next->coder->rc);

	// State
	next->coder->state = 0;
	next->coder->previous_byte = 0;
	for (size_t i = 0; i < REP_DISTANCES; ++i)
		next->coder->rep_distances[i] = 0;

	// Bit encoders
	for (size_t i = 0; i < STATES; ++i) {
		for (size_t j = 0; j <= next->coder->pos_mask; ++j) {
			bit_reset(next->coder->is_match[i][j]);
			bit_reset(next->coder->is_rep0_long[i][j]);
		}

		bit_reset(next->coder->is_rep[i]);
		bit_reset(next->coder->is_rep0[i]);
		bit_reset(next->coder->is_rep1[i]);
		bit_reset(next->coder->is_rep2[i]);
	}

	for (size_t i = 0; i < FULL_DISTANCES - END_POS_MODEL_INDEX; ++i)
		bit_reset(next->coder->pos_encoders[i]);

	// Bit tree encoders
	for (size_t i = 0; i < LEN_TO_POS_STATES; ++i)
		bittree_reset(next->coder->pos_slot_encoder[i], POS_SLOT_BITS);

	bittree_reset(next->coder->pos_align_encoder, ALIGN_BITS);

	// Length encoders
	length_encoder_reset(&next->coder->len_encoder, 1U << options->pos_bits,
			options->fast_bytes + 1 - MATCH_MIN_LEN);

	length_encoder_reset(&next->coder->rep_match_len_encoder,
			1U << options->pos_bits,
			next->coder->fast_bytes + 1 - MATCH_MIN_LEN);

	// Misc
	next->coder->longest_match_was_found = false;
	next->coder->optimum_end_index = 0;
	next->coder->optimum_current_index = 0;
	next->coder->additional_offset = 0;

	next->coder->now_pos = 0;
	next->coder->is_initialized = false;

	// Initialize the next decoder in the chain, if any.
	{
		const lzma_ret ret = lzma_next_filter_init(&next->coder->next,
				allocator, filters + 1);
		if (ret != LZMA_OK) {
			lzma_lzma_encoder_end(next->coder, allocator);
			return ret;
		}
	}

	// Initialization successful. Set the function pointers.
	next->code = &lzma_lz_encode;
	next->end = &lzma_lzma_encoder_end;

	return LZMA_OK;
}


extern bool
lzma_lzma_encode_properties(const lzma_options_lzma *options, uint8_t *byte)
{
	if (options->literal_context_bits > LZMA_LITERAL_CONTEXT_BITS_MAX
			|| options->literal_pos_bits
				> LZMA_LITERAL_POS_BITS_MAX
			|| options->pos_bits > LZMA_POS_BITS_MAX)
		return true;

	*byte = (options->pos_bits * 5 + options->literal_pos_bits) * 9
			+ options->literal_context_bits;
	assert(*byte <= (4 * 5 + 4) * 9 + 8);

	return false;
}
