///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_decoder.c
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

#include "lz_decoder.h"


/// Minimum size of allocated dictionary
#define DICT_SIZE_MIN 8192

/// When there is less than this amount of data available for decoding,
/// it is moved to the temporary buffer which
///   - protects from reads past the end of the buffer; and
///   - stored the incomplete data between lzma_code() calls.
///
/// \note       TEMP_LIMIT must be at least as much as
///             REQUIRED_IN_BUFFER_SIZE defined in lzma_decoder.c.
#define TEMP_LIMIT 32

// lzma_lz_decoder.dict[] must be three times the size of TEMP_LIMIT.
// 2 * TEMP_LIMIT is used for the actual data, and the third TEMP_LIMIT
// bytes is needed for safety to allow decode_dummy() in lzma_decoder.c
// to read past end of the buffer. This way it should be both fast and simple.
#if LZMA_BUFFER_SIZE < 3 * TEMP_LIMIT
#	error LZMA_BUFFER_SIZE < 3 * TEMP_LIMIT
#endif


struct lzma_coder_s {
	lzma_next_coder next;
	lzma_lz_decoder lz;

	// There are more members in this structure but they are not
	// visible in LZ coder.
};


/// - Copy as much data as possible from lz->dict[] to out[].
/// - Update *out_pos, lz->start, and lz->end accordingly.
/// - Wrap lz-pos to the beginning of lz->dict[] if there is a danger that
///   it may go past the end of the buffer (lz->pos >= lz->must_flush_pos).
static inline bool
flush(lzma_lz_decoder *restrict lz, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size)
{
	// Flush uncompressed data from the history buffer to
	// the output buffer. This is done in two phases.

	assert(lz->start <= lz->end);

	// Flush if pos < start < end.
	if (lz->pos < lz->start && lz->start < lz->end) {
		bufcpy(lz->dict, &lz->start, lz->end, out, out_pos, out_size);

		// If we reached end of the data in history buffer,
		// wrap to the beginning.
		if (lz->start == lz->end)
			lz->start = 0;
	}

	// Flush if start start < pos <= end. This is not as `else' for
	// previous `if' because the previous one may make this one true.
	if (lz->start < lz->pos) {
		bufcpy(lz->dict, &lz->start,
				lz->pos, out, out_pos, out_size);

		if (lz->pos >= lz->must_flush_pos) {
			// Wrap the flushing position if we have
			// flushed the whole history buffer.
			if (lz->pos == lz->start)
				lz->start = 0;

			// Wrap the write position and store to lz.end
			// how much there is new data available.
			lz->end = lz->pos;
			lz->pos = 0;
			lz->is_full = true;
		}
	}

	assert(lz->pos < lz->must_flush_pos);

	return *out_pos == out_size;
}


/// Calculate safe value for lz->limit. If no safe value can be found,
/// set lz->limit to zero. When flushing, only as little data will be
/// decoded as is needed to fill the output buffer (lowers both latency
/// and throughput).
///
/// \return     true if there is no space for new uncompressed data.
///
static inline bool
set_limit(lzma_lz_decoder *lz, size_t out_avail, bool flushing)
{
	// Set the limit so that writing to dict[limit + match_max_len - 1]
	// doesn't overwrite any unflushed data and doesn't write past the
	// end of the dict buffer.
	if (lz->start <= lz->pos) {
		// We can fill the buffer from pos till the end
		// of the dict buffer.
		lz->limit = lz->must_flush_pos;
	} else if (lz->pos + lz->match_max_len < lz->start) {
		// There's some unflushed data between pos and end of the
		// buffer. Limit so that we don't overwrite the unflushed data.
		lz->limit = lz->start - lz->match_max_len;
	} else {
		// Buffer is too full.
		lz->limit = 0;
		return true;
	}

	// Finetune the limit a bit if it isn't zero.

	assert(lz->limit > lz->pos);
	const size_t dict_avail = lz->limit - lz->pos;

	if (lz->uncompressed_size < dict_avail) {
		// Finishing a stream that doesn't have
		// an end of stream marker.
		lz->limit = lz->pos + lz->uncompressed_size;

	} else if (flushing && out_avail < dict_avail) {
		// Flushing enabled, decoding only as little as needed to
		// fill the out buffer (if there's enough input, of course).
		lz->limit = lz->pos + out_avail;
	}

	return lz->limit == lz->pos;
}


/// Takes care of wrapping the data into temporary buffer when needed,
/// and calls the actual decoder.
///
/// \return     true if error occurred
///
static inline bool
call_process(lzma_coder *restrict coder, const uint8_t *restrict in,
		size_t *restrict in_pos, size_t in_size)
{
	// It would be nice and simple if we could just give in[] to the
	// decoder, but the requirement of zlib-like API forces us to be
	// able to make *in_pos == in_size whenever there is enough output
	// space. If needed, we will append a few bytes from in[] to
	// a temporary buffer and decode enough to reach the part that
	// was copied from in[]. Then we can continue with the real in[].

	bool error;
	const size_t dict_old_pos = coder->lz.pos;
	const size_t in_avail = in_size - *in_pos;

	if (coder->lz.temp_size + in_avail < 2 * TEMP_LIMIT) {
		// Copy all the available input from in[] to temp[].
		memcpy(coder->lz.temp + coder->lz.temp_size,
				in + *in_pos, in_avail);
		coder->lz.temp_size += in_avail;
		*in_pos += in_avail;
		assert(*in_pos == in_size);

		// Decode as much as possible.
		size_t temp_used = 0;
		error = coder->lz.process(coder, coder->lz.temp, &temp_used,
				coder->lz.temp_size, true);
		assert(temp_used <= coder->lz.temp_size);

		// Move the remaining data to the beginning of temp[].
		coder->lz.temp_size -= temp_used;
		memmove(coder->lz.temp, coder->lz.temp + temp_used,
				coder->lz.temp_size);

	} else if (coder->lz.temp_size > 0) {
		// Fill temp[] unless it is already full because we aren't
		// the last filter in the chain.
		size_t copy_size = 0;
		if (coder->lz.temp_size < 2 * TEMP_LIMIT) {
			assert(*in_pos < in_size);
			copy_size = 2 * TEMP_LIMIT - coder->lz.temp_size;
			memcpy(coder->lz.temp + coder->lz.temp_size,
					in + *in_pos, copy_size);
			// NOTE: We don't update lz.temp_size or *in_pos yet.
		}

		size_t temp_used = 0;
		error = coder->lz.process(coder, coder->lz.temp, &temp_used,
				coder->lz.temp_size + copy_size, false);

		if (temp_used < coder->lz.temp_size) {
			// Only very little input data was consumed. Move
			// the unprocessed data to the beginning temp[].
			coder->lz.temp_size += copy_size - temp_used;
			memmove(coder->lz.temp, coder->lz.temp + temp_used,
					coder->lz.temp_size);
			*in_pos += copy_size;
			assert(*in_pos <= in_size);

		} else {
			// We were able to decode so much data that next time
			// we can decode directly from in[]. That is, we can
			// consider temp[] to be empty now.
			*in_pos += temp_used - coder->lz.temp_size;
			coder->lz.temp_size = 0;
			assert(*in_pos <= in_size);
		}

	} else {
		// Decode directly from in[].
		error = coder->lz.process(coder, in, in_pos, in_size, false);
		assert(*in_pos <= in_size);
	}

	assert(coder->lz.pos >= dict_old_pos);
	if (coder->lz.uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		// Update uncompressed size.
		coder->lz.uncompressed_size -= coder->lz.pos - dict_old_pos;

		// Check that End of Payload Marker hasn't been detected
		// since it must not be present because uncompressed size
		// is known.
		if (coder->lz.eopm_detected)
			error = true;
	}

	return error;
}


static lzma_ret
decode_buffer(lzma_coder *coder,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		bool flushing)
{
	bool stop = false;

	while (true) {
		// Flush from coder->lz.dict to out[].
		flush(&coder->lz, out, out_pos, out_size);

		// All done?
		if (*out_pos == out_size
				|| stop
				|| coder->lz.eopm_detected
				|| coder->lz.uncompressed_size == 0)
			break;

		// Set write limit in the dictionary.
		if (set_limit(&coder->lz, out_size - *out_pos, flushing))
			break;

		// Decode more data.
		if (call_process(coder, in, in_pos, in_size))
			return LZMA_DATA_ERROR;

		// Set stop to true if we must not call call_process() again
		// during this function call.
		// FIXME: Can this make the loop exist too early? It wouldn't
		// cause data corruption so not a critical problem. It can
		// happen if dictionary gets full and lz.temp still contains
		// a few bytes data that we could decode right now.
		if (*in_pos == in_size && coder->lz.temp_size <= TEMP_LIMIT
				&& coder->lz.pos < coder->lz.limit)
			stop = true;
	}

	// If we have decoded everything (EOPM detected or uncompressed_size
	// bytes were processed) to the history buffer, and also flushed
	// everything from the history buffer, our job is done.
	if ((coder->lz.eopm_detected
			|| coder->lz.uncompressed_size == 0)
			&& coder->lz.start == coder->lz.pos)
		return LZMA_STREAM_END;

	return LZMA_OK;
}


extern lzma_ret
lzma_lz_decode(lzma_coder *coder,
		lzma_allocator *allocator lzma_attribute((unused)),
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action)
{
	if (coder->next.code == NULL) {
		const lzma_ret ret = decode_buffer(coder, in, in_pos, in_size,
				out, out_pos, out_size,
				action == LZMA_SYNC_FLUSH);

		if (*out_pos == out_size || ret == LZMA_STREAM_END) {
			// Unread to make coder->temp[] empty. This is easy,
			// because we know that all the data currently in
			// coder->temp[] has been copied form in[] during this
			// call to the decoder.
			//
			// If we didn't do this, we could have data left in
			// coder->temp[] when end of stream is reached. That
			// data could be left there from *previous* call to
			// the decoder; in that case we wouldn't know where
			// to put that data.
			assert(*in_pos >= coder->lz.temp_size);
			*in_pos -= coder->lz.temp_size;
			coder->lz.temp_size = 0;
		}

		return ret;
	}

	// We aren't the last coder in the chain, we need to decode
	// our input to a temporary buffer.
	const bool flushing = action == LZMA_SYNC_FLUSH;
	while (*out_pos < out_size) {
		if (!coder->lz.next_finished
				&& coder->lz.temp_size < LZMA_BUFFER_SIZE) {
			const lzma_ret ret = coder->next.code(
					coder->next.coder,
					allocator, in, in_pos, in_size,
					coder->lz.temp, &coder->lz.temp_size,
					LZMA_BUFFER_SIZE, action);

			if (ret == LZMA_STREAM_END)
				coder->lz.next_finished = true;
			else if (coder->lz.temp_size < LZMA_BUFFER_SIZE
					|| ret != LZMA_OK)
				return ret;
		}

		if (coder->lz.this_finished) {
			if (coder->lz.temp_size != 0)
				return LZMA_DATA_ERROR;

			if (coder->lz.next_finished)
				return LZMA_STREAM_END;

			return LZMA_OK;
		}

		size_t dummy = 0;
		const lzma_ret ret = decode_buffer(coder, NULL, &dummy, 0,
				out, out_pos, out_size, flushing);

		if (ret == LZMA_STREAM_END)
			coder->lz.this_finished = true;
		else if (ret != LZMA_OK)
			return ret;
		else if (coder->lz.next_finished && *out_pos < out_size)
			return LZMA_DATA_ERROR;
	}

	return LZMA_OK;
}


/// \brief      Initializes LZ part of the LZMA decoder or Inflate
///
/// \param      history_size    Number of bytes the LZ out window is
///                             supposed keep available from the output
///                             history.
/// \param      match_max_len   Number of bytes a single decoding loop
///                             can advance the write position (lz->pos)
///                             in the history buffer (lz->dict).
///
/// \note       This function is called by LZMA decoder and Inflate init()s.
///             It's up to those functions allocate *lz and initialize it
///             with LZMA_LZ_DECODER_INIT.
extern lzma_ret
lzma_lz_decoder_reset(lzma_lz_decoder *lz, lzma_allocator *allocator,
		bool (*process)(lzma_coder *restrict coder,
			const uint8_t *restrict in, size_t *restrict in_pos,
			size_t in_size, bool has_safe_buffer),
		lzma_vli uncompressed_size,
		size_t history_size, size_t match_max_len)
{
	// Set uncompressed size.
	lz->uncompressed_size = uncompressed_size;

	// Limit the history size to roughly sane values. This is primarily
	// to prevent integer overflows.
	if (history_size > UINT32_MAX / 2)
		return LZMA_HEADER_ERROR;

	// Store the value actually requested. We use it for sanity checks
	// when repeating data from the history buffer.
	lz->requested_size = history_size;

	// Avoid tiny history buffer sizes for performance reasons.
	// TODO: Test if this actually helps...
	if (history_size < DICT_SIZE_MIN)
		history_size = DICT_SIZE_MIN;

	// The real size of the history buffer is a bit bigger than
	// requested by our caller. This allows us to do some optimizations,
	// which help not only speed but simplicity of the code; specifically,
	// we can make sure that there is always at least match_max_len
	// bytes immediatelly available for writing without a need to wrap
	// the history buffer.
	const size_t dict_real_size = history_size + 2 * match_max_len + 1;

	// Reallocate memory if needed.
	if (history_size != lz->size || match_max_len != lz->match_max_len) {
		// Destroy the old buffer.
		lzma_lz_decoder_end(lz, allocator);

		lz->size = history_size;
		lz->match_max_len = match_max_len;
		lz->must_flush_pos = history_size + match_max_len + 1;

		lz->dict = lzma_alloc(dict_real_size, allocator);
		if (lz->dict == NULL)
			return LZMA_MEM_ERROR;
	}

	// Reset the variables so that lz_get_byte(lz, 0) will return '\0'.
	lz->pos = 0;
	lz->start = 0;
	lz->end = dict_real_size;
	lz->dict[dict_real_size - 1] = 0;
	lz->is_full = false;
	lz->eopm_detected = false;
	lz->next_finished = false;
	lz->this_finished = false;
	lz->temp_size = 0;

	// Clean up the temporary buffer to make it very sure that there are
	// no information leaks when multiple steams are decoded with the
	// same decoder structures.
	memzero(lz->temp, LZMA_BUFFER_SIZE);

	// Set the process function pointer.
	lz->process = process;

	return LZMA_OK;
}


extern void
lzma_lz_decoder_end(lzma_lz_decoder *lz, lzma_allocator *allocator)
{
	lzma_free(lz->dict, allocator);
	lz->dict = NULL;
	lz->size = 0;
	lz->match_max_len = 0;
	return;
}
