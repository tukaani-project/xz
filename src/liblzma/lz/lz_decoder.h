///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_decoder.h
/// \brief      LZ out window
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

#ifndef LZMA_LZ_OUT_H
#define LZMA_LZ_OUT_H

#include "common.h"


/// Get a byte from the history buffer.
#define lz_get_byte(lz, distance) \
	((distance) < (lz).pos \
		? (lz).dict[(lz).pos - (distance) - 1] \
		: (lz).dict[(lz).pos - (distance) - 1 + (lz).end])


#define LZMA_LZ_DECODER_INIT \
	(lzma_lz_decoder){ .dict = NULL, .size = 0, .match_max_len = 0 }


typedef struct {
	/// Function to do the actual decoding (LZMA or Inflate)
	bool (*process)(lzma_coder *restrict coder, const uint8_t *restrict in,
			size_t *restrict in_pos, size_t size_in,
			bool has_safe_buffer);

	/// Pointer to dictionary (history) buffer.
	/// \note Not 'restrict' because can alias next_out.
	uint8_t *dict;

	/// Next write goes to dict[pos].
	size_t pos;

	/// Next byte to flush is buffer[start].
	size_t start;

	/// First byte to not flush is buffer[end].
	size_t end;

	/// First position to which data must not be written.
	size_t limit;

	/// True if dictionary has needed wrapping.
	bool is_full;

	/// True if process() has detected End of Payload Marker.
	bool eopm_detected;

	/// True if the next coder in the chain has returned LZMA_STREAM_END.
	bool next_finished;

	/// True if the LZ decoder (e.g. LZMA) has detected End of Payload
	/// Marker. This may become true before next_finished becomes true.
	bool this_finished;

	/// When pos >= must_flush_pos, we must not call process().
	size_t must_flush_pos;

	/// Maximum number of bytes that a single decoding loop inside
	/// process() can produce data into dict. This amount is kept
	/// always available at dict + pos i.e. it is safe to write a byte
	/// to dict[pos + match_max_len - 1].
	size_t match_max_len;

	/// Number of bytes allocated to dict.
	size_t size;

	/// Requested size of the dictionary. This is needed because we avoid
	/// using extremely tiny history buffers.
	size_t requested_size;

	/// Uncompressed Size or LZMA_VLI_VALUE_UNKNOWN if unknown.
	lzma_vli uncompressed_size;

	/// Number of bytes currently in temp[].
	size_t temp_size;

	/// Temporary buffer needed when
	/// 1) we cannot make the input buffer completely empty; or
	/// 2) we are not the last filter in the chain.
	uint8_t temp[LZMA_BUFFER_SIZE];

} lzma_lz_decoder;


/////////////////////////
// Function prototypes //
/////////////////////////

extern lzma_ret lzma_lz_decoder_reset(lzma_lz_decoder *lz,
		lzma_allocator *allocator, bool (*process)(
			lzma_coder *restrict coder, const uint8_t *restrict in,
			size_t *restrict in_pos, size_t in_size,
			bool has_safe_buffer),
		lzma_vli uncompressed_size,
		size_t history_size, size_t match_max_len);

extern lzma_ret lzma_lz_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action);

/// Deallocates the history buffer if one exists.
extern void lzma_lz_decoder_end(
		lzma_lz_decoder *lz, lzma_allocator *allocator);

//////////////////////
// Inline functions //
//////////////////////

// Repeat a block of data from the history. Because memcpy() is faster
// than copying byte by byte in a loop, the copying process gets split
// into three cases:
// 1. distance < length
//    Source and target areas overlap, thus we can't use memcpy()
//    (nor memmove()) safely.
//    TODO: If this is common enough, it might be worth optimizing this
//    more e.g. by checking if distance > sizeof(uint8_t*) and using
//    memcpy in small chunks.
// 2. distance < pos
//    This is the easiest and the fastest case. The block being copied
//    is a contiguous piece in the history buffer. The buffer offset
//    doesn't need wrapping.
// 3. distance >= pos
//    We need to wrap the position, because otherwise we would try copying
//    behind the first byte of the allocated buffer. It is possible that
//    the block is fragmeneted into two pieces, thus we might need to call
//    memcpy() twice.
// NOTE: The function using this macro must ensure that length is positive
// and that distance is FIXME
static inline bool
lzma_lz_out_repeat(lzma_lz_decoder *lz, size_t distance, size_t length)
{
	// Validate offset of the block to be repeated. It doesn't
	// make sense to copy data behind the beginning of the stream.
	// Leaving this check away would lead to a security problem,
	// in which e.g. the data of the previously decoded file(s)
	// would be leaked (or whatever happens to be in unused
	// part of the dictionary buffer).
	if (distance >= lz->pos && !lz->is_full)
		return false;

	// It also doesn't make sense to copy data farer than
	// the dictionary size.
	if (distance >= lz->requested_size)
		return false;

	// The caller must have checked these!
	assert(distance <= lz->size);
	assert(length > 0);
	assert(length <= lz->match_max_len);

	// Copy the amount of data requested by the decoder.
	if (distance < length) {
		// Source and target areas overlap, thus we can't use
		// memcpy() nor even memmove() safely. :-(
		// TODO: Copying byte by byte is slow. It might be
		// worth optimizing this more if this case is common.
		do {
			lz->dict[lz->pos] = lz_get_byte(*lz, distance);
			++lz->pos;
		} while (--length > 0);

	} else if (distance < lz->pos) {
		// The easiest and fastest case
		memcpy(lz->dict + lz->pos,
				lz->dict + lz->pos - distance - 1,
				length);
		lz->pos += length;

	} else {
		// The bigger the dictionary, the more rare this
		// case occurs. We need to "wrap" the dict, thus
		// we might need two memcpy() to copy all the data.
		assert(lz->is_full);
		const uint32_t copy_pos = lz->pos - distance - 1 + lz->end;
		uint32_t copy_size = lz->end - copy_pos;

		if (copy_size < length) {
			memcpy(lz->dict + lz->pos, lz->dict + copy_pos,
					copy_size);
			lz->pos += copy_size;
			copy_size = length - copy_size;
			memcpy(lz->dict + lz->pos, lz->dict, copy_size);
			lz->pos += copy_size;
		} else {
			memcpy(lz->dict + lz->pos, lz->dict + copy_pos,
					length);
			lz->pos += length;
		}
	}

	return true;
}

#endif
