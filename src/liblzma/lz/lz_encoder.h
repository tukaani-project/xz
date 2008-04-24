///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder.h
/// \brief      LZ in window and match finder API
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

#ifndef LZMA_LZ_ENCODER_H
#define LZMA_LZ_ENCODER_H

#include "common.h"


#define LZMA_LZ_TEMP_SIZE 64


typedef struct lzma_lz_encoder_s lzma_lz_encoder;
struct lzma_lz_encoder_s {
	enum {
		SEQ_START,
		SEQ_RUN,
		SEQ_FLUSH,
		SEQ_FLUSH_END,
		SEQ_FINISH,
		SEQ_END
	} sequence;

	bool (*process)(lzma_coder *coder, uint8_t *restrict out,
			size_t *restrict out_pos, size_t out_size);

	/// Uncompressed Size or LZMA_VLI_VALUE_UNKNOWN if using EOPM. We need
	/// to track Uncompressed Size to prevent writing flush marker to the
	/// very end of stream that doesn't use EOPM.
	lzma_vli uncompressed_size;

	/// Temporary buffer for range encoder.
	uint8_t temp[LZMA_LZ_TEMP_SIZE];
	size_t temp_size;

	///////////////
	// In Window //
	///////////////

	/// Pointer to buffer with data to be compressed
	uint8_t *buffer;

	/// Total size of the allocated buffer (that is, including all
	/// the extra space)
	size_t size;

	/// Match finders store locations of matches using 32-bit integers.
	/// To avoid adjusting several megabytes of integers every time the
	/// input window is moved with move_window(), we only adjust the
	/// offset of the buffer. Thus, buffer[match_finder_pos - offset]
	/// is the byte pointed by match_finder_pos.
	size_t offset;

	/// buffer[read_pos] is the current byte.
	size_t read_pos;

	/// As long as read_pos is less than read_limit, there is enough
	/// input available in buffer for at least one encoding loop.
	///
	/// Because of the stateful API, read_limit may and will get greater
	/// than read_pos quite often. This is taken into account when
	/// calculating the value for keep_size_after.
	size_t read_limit;

	/// buffer[write_pos] is the first byte that doesn't contain valid
	/// uncompressed data; that is, the next input byte will be copied
	/// to buffer[write_pos].
	size_t write_pos;

	/// Number of bytes not hashed before read_pos. This is needed to
	/// restart the match finder after LZMA_SYNC_FLUSH.
	size_t pending;

	/// Number of bytes that must be kept available in our input history.
	/// That is, once keep_size_before bytes have been processed,
	/// buffer[read_pos - keep_size_before] is the oldest byte that
	/// must be available for reading.
	size_t keep_size_before;

	/// Number of bytes that must be kept in buffer after read_pos.
	/// That is, read_pos <= write_pos - keep_size_after as long as
	/// stream_end_was_reached is false (once it is true, read_pos
	/// is allowed to reach write_pos).
	size_t keep_size_after;

	//////////////////
	// Match Finder //
	//////////////////

	// Pointers to match finder functions
	void (*get_matches)(lzma_lz_encoder *restrict lz,
			uint32_t *restrict distances);
	void (*skip)(lzma_lz_encoder *restrict lz, uint32_t num);

	// Match finder data
	uint32_t *hash; // TODO: Check if hash aliases son
	uint32_t *son;  //       and add 'restrict' if possible.
	uint32_t cyclic_buffer_pos;
	uint32_t cyclic_buffer_size; // Must be dictionary_size + 1.
	uint32_t hash_mask;
	uint32_t cut_value;
	uint32_t hash_size_sum;
	uint32_t num_items;
	uint32_t match_max_len;
};


#define LZMA_LZ_ENCODER_INIT \
	(lzma_lz_encoder){ \
		.buffer = NULL, \
		.size = 0, \
		.hash = NULL, \
		.num_items = 0, \
	}


/// Calculates
extern bool lzma_lz_encoder_hash_properties(lzma_match_finder match_finder,
		uint32_t history_size, uint32_t *restrict hash_mask,
		uint32_t *restrict hash_size_sum,
		uint32_t *restrict num_items);

// NOTE: liblzma doesn't use callback API like LZMA SDK does. The caller
// must make sure that keep_size_after is big enough for single encoding pass
// i.e. keep_size_after >= maximum number of bytes possibly needed after
// the current position between calls to lzma_lz_read().
extern lzma_ret lzma_lz_encoder_reset(lzma_lz_encoder *lz,
		lzma_allocator *allocator,
		bool (*process)(lzma_coder *coder, uint8_t *restrict out,
			size_t *restrict out_pos, size_t out_size),
		lzma_vli uncompressed_size,
		size_t history_size, size_t additional_buffer_before,
		size_t match_max_len, size_t additional_buffer_after,
		lzma_match_finder match_finder, uint32_t match_finder_cycles,
		const uint8_t *preset_dictionary,
		size_t preset_dictionary_size);

/// Frees memory allocated for in window and match finder buffers.
extern void lzma_lz_encoder_end(
		lzma_lz_encoder *lz, lzma_allocator *allocator);

extern lzma_ret lzma_lz_encode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action);

/// This should not be called directly, but only via move_pos() macro.
extern void lzma_lz_encoder_normalize(lzma_lz_encoder *lz);

#endif
