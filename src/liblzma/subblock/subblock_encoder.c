///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_encoder.c
/// \brief      Encoder of the Subblock filter
//
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

#include "subblock_encoder.h"
#include "raw_encoder.h"


#define REPEAT_COUNT_MAX (1U << 28)

/// Number of bytes the data chunk being repeated must be before we care
/// about alignment. This is somewhat arbitrary. It just doesn't make sense
/// to waste bytes for alignment when the data chunk is very small.
///
/// TODO Rename and use this also for Subblock Data?
#define RLE_MIN_SIZE_FOR_ALIGN 3

#define write_byte(b) \
do { \
	out[*out_pos] = b; \
	++*out_pos; \
	++coder->alignment.out_pos; \
} while (0)


struct lzma_coder_s {
	lzma_next_coder next;
	bool next_finished;

	enum {
		SEQ_FILL,
		SEQ_FLUSH,
		SEQ_RLE_COUNT_0,
		SEQ_RLE_COUNT_1,
		SEQ_RLE_COUNT_2,
		SEQ_RLE_COUNT_3,
		SEQ_RLE_SIZE,
		SEQ_RLE_DATA,
		SEQ_DATA_SIZE_0,
		SEQ_DATA_SIZE_1,
		SEQ_DATA_SIZE_2,
		SEQ_DATA_SIZE_3,
		SEQ_DATA,
		SEQ_SUBFILTER_INIT,
		SEQ_SUBFILTER_FLAGS,
	} sequence;

	lzma_options_subblock *options;

	lzma_vli uncompressed_size;

	size_t pos;
	uint32_t tmp;

	struct {
		uint32_t multiple;
		uint32_t in_pending;
		uint32_t in_pos;
		uint32_t out_pos;
	} alignment;

	struct {
		uint8_t *data;
		size_t size;
		size_t limit;
	} subblock;

	struct {
		uint8_t buffer[LZMA_SUBBLOCK_RLE_MAX];
		size_t size;
		lzma_vli count;
	} rle;

	struct {
		enum {
			SUB_NONE,
			SUB_SET,
			SUB_RUN,
			SUB_FINISH,
			SUB_END_MARKER,
		} mode;

		bool got_input;

		uint8_t *flags;
		uint32_t flags_size;

		lzma_next_coder subcoder;

	} subfilter;

	struct {
		size_t pos;
		size_t size;
		uint8_t buffer[LZMA_BUFFER_SIZE];
	} temp;
};


/// \brief      Aligns the output buffer
///
/// Aligns the output buffer so that after skew bytes the output position is
/// a multiple of coder->alignment.multiple.
static bool
subblock_align(lzma_coder *coder, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, uint32_t skew)
{
	assert(*out_pos < out_size);

	const uint32_t target = coder->alignment.in_pos
			% coder->alignment.multiple;

	while ((coder->alignment.out_pos + skew)
			% coder->alignment.multiple != target) {
		// Zero indicates padding.
		write_byte(0x00);

		// Check if output buffer got full and indicate it to
		// the caller.
		if (*out_pos == out_size)
			return true;
	}

	coder->alignment.in_pos += coder->alignment.in_pending;
	coder->alignment.in_pending = 0;

	// Output buffer is not full.
	return false;
}


/// \brief      Checks if buffer contains repeated data
///
/// \param      needle      Buffer containing a single repeat chunk
/// \param      needle_size Size of needle in bytes
/// \param      buf         Buffer to search for repeated needles
/// \param      buf_chunks  Buffer size is buf_chunks * needle_size.
///
/// \return     True if the whole buf is filled with repeated needles.
///
static bool
is_repeating(const uint8_t *restrict needle, size_t needle_size,
		const uint8_t *restrict buf, size_t buf_chunks)
{
	while (buf_chunks-- != 0) {
		if (memcmp(buf, needle, needle_size) != 0)
			return false;

		buf += needle_size;
	}

	return true;
}


/// \brief      Optimizes the repeating style and updates coder->sequence
static void
subblock_rle_flush(lzma_coder *coder)
{
	// The Subblock decoder can use memset() when the size of the data
	// being repeated is one byte, so we check if the RLE buffer is
	// filled with a single repeating byte.
	if (coder->rle.size > 1) {
		const uint8_t b = coder->rle.buffer[0];
		size_t i = 0;
		while (true) {
			if (coder->rle.buffer[i] != b)
				break;

			if (++i == coder->rle.size) {
				// TODO Integer overflow check maybe,
				// although this needs at least 2**63 bytes
				// of input until it gets triggered...
				coder->rle.count *= coder->rle.size;
				coder->rle.size = 1;
				break;
			}
		}
	}

	if (coder->rle.count > REPEAT_COUNT_MAX)
		coder->tmp = REPEAT_COUNT_MAX - 1;
	else
		coder->tmp = coder->rle.count - 1;

	coder->sequence = SEQ_RLE_COUNT_0;

	return;
}


/// \brief      Resizes coder->subblock.data for a new size limit
static lzma_ret
subblock_data_size(lzma_coder *coder, lzma_allocator *allocator,
		size_t new_limit)
{
	// Verify that the new limit is valid.
	if (new_limit < LZMA_SUBBLOCK_DATA_SIZE_MIN
			|| new_limit > LZMA_SUBBLOCK_DATA_SIZE_MAX)
		return LZMA_HEADER_ERROR;

	// Ff the new limit is different than the previous one, we need
	// to reallocate the data buffer.
	if (new_limit != coder->subblock.limit) {
		lzma_free(coder->subblock.data, allocator);
		coder->subblock.data = lzma_alloc(new_limit, allocator);
		if (coder->subblock.data == NULL)
			return LZMA_MEM_ERROR;
	}

	coder->subblock.limit = new_limit;

	return LZMA_OK;
}


static lzma_ret
subblock_buffer(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Verify that there is a sane amount of input.
	if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		const lzma_vli in_avail = in_size - *in_pos;
		if (action == LZMA_FINISH) {
			if (in_avail != coder->uncompressed_size)
				return LZMA_DATA_ERROR;
		} else {
			if (in_avail > coder->uncompressed_size)
				return LZMA_DATA_ERROR;
		}
	}

	// Check if we need to do something special with the Subfilter.
	if (coder->options != NULL && coder->options->allow_subfilters) {
		switch (coder->options->subfilter_mode) {
		case LZMA_SUBFILTER_NONE:
			if (coder->subfilter.mode != SUB_NONE)
				return LZMA_PROG_ERROR;
			break;

		case LZMA_SUBFILTER_SET:
			if (coder->subfilter.mode != SUB_NONE)
				return LZMA_HEADER_ERROR;

			coder->subfilter.mode = SUB_SET;
			coder->subfilter.got_input = false;

			if (coder->sequence == SEQ_FILL)
				coder->sequence = SEQ_FLUSH;

			break;

		case LZMA_SUBFILTER_RUN:
			if (coder->subfilter.mode != SUB_RUN)
				return LZMA_PROG_ERROR;
			break;

		case LZMA_SUBFILTER_FINISH:
			if (coder->subfilter.mode == SUB_RUN)
				coder->subfilter.mode = SUB_FINISH;
			else if (coder->subfilter.mode != SUB_FINISH)
				return LZMA_PROG_ERROR;

			if (!coder->subfilter.got_input)
				return LZMA_PROG_ERROR;

			break;

		default:
			return LZMA_HEADER_ERROR;
		}
	}

	// Main loop
	while (*out_pos < out_size)
	switch (coder->sequence) {
	case SEQ_FILL: {
		// Grab the new Subblock Data Size and reallocate the buffer.
		if (coder->subblock.size == 0 && coder->options != NULL
				&& coder->options->subblock_data_size
					!= coder->subblock.limit)
			return_if_error(subblock_data_size(coder,
					allocator, coder->options
						->subblock_data_size));

		if (coder->subfilter.mode == SUB_NONE) {
			assert(coder->subfilter.subcoder.code == NULL);

			// No Subfilter is enabled, just copy the data as is.
			// NOTE: uncompressed_size cannot overflow because we
			// have checked/ it in the beginning of this function.
			const size_t in_used = bufcpy(in, in_pos, in_size,
					coder->subblock.data,
					&coder->subblock.size,
					coder->subblock.limit);

			if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
				coder->uncompressed_size -= in_used;

			coder->alignment.in_pending += in_used;

		} else {
			const size_t in_start = *in_pos;
			lzma_ret ret;

			if (coder->subfilter.mode == SUB_FINISH) {
				// Let the Subfilter write out pending data,
				// but don't give it any new input anymore.
				size_t dummy = 0;
				ret = coder->subfilter.subcoder.code(coder
						->subfilter.subcoder.coder,
						allocator, NULL, &dummy, 0,
						coder->subblock.data,
						&coder->subblock.size,
						coder->subblock.limit,
						LZMA_FINISH);
			} else {
				// Give our input data to the Subfilter. Note
				// that action can be LZMA_FINISH. In that
				// case, we filter everything until the end
				// of the input. The application isn't required
				// to separately set LZMA_SUBBLOCK_FINISH.
				ret = coder->subfilter.subcoder.code(coder
						->subfilter.subcoder.coder,
						allocator, in, in_pos, in_size,
						coder->subblock.data,
						&coder->subblock.size,
						coder->subblock.limit,
						action);
			}

			const size_t in_used = *in_pos - in_start;

			if (in_used > 0)
				coder->subfilter.got_input = true;

			// NOTE: uncompressed_size cannot overflow because we
			// have checked it in the beginning of this function.
			if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
				coder->uncompressed_size -= *in_pos - in_start;

			coder->alignment.in_pending += in_used;

			if (ret == LZMA_STREAM_END) {
				// We don't strictly need to do this, but
				// doing it sounds like a good idea, because
				// otherwise the Subfilter's memory could be
				// left allocated for long time, and would
				// just waste memory.
				lzma_next_coder_end(&coder->subfilter.subcoder,
						allocator);

				assert(coder->options != NULL);
				coder->options->subfilter_mode
						= LZMA_SUBFILTER_NONE;

				assert(coder->subfilter.mode == SUB_FINISH
						|| action == LZMA_FINISH);
				coder->subfilter.mode = SUB_END_MARKER;

				// Flush now. Even if coder->subblock.size
				// happens to be zero, we still need to go
				// to SEQ_FLUSH to write the Subfilter Unset
				// indicator.
				coder->sequence = SEQ_FLUSH;
				break;
			}

			// Return if an error occurred.
			if (ret != LZMA_OK)
				return ret;
		}

		// If we ran out of input before the whole buffer
		// was filled, return to application.
		if (coder->subblock.size < coder->subblock.limit
				&& action != LZMA_FINISH)
			return LZMA_OK;

		coder->sequence = SEQ_FLUSH;
	}

	// Fall through

	case SEQ_FLUSH:
		if (coder->options != NULL) {
			// Update the alignment variable.
			coder->alignment.multiple = coder->options->alignment;
			if (coder->alignment.multiple
					< LZMA_SUBBLOCK_ALIGNMENT_MIN
					|| coder->alignment.multiple
					> LZMA_SUBBLOCK_ALIGNMENT_MAX)
				return LZMA_HEADER_ERROR;

			// Run-length encoder
			//
			// First check if there is some data pending and we
			// have an obvious need to flush it immediatelly.
			if (coder->rle.count > 0
					&& (coder->rle.size
							!= coder->options->rle
						|| coder->subblock.size
							% coder->rle.size)) {
				subblock_rle_flush(coder);
				break;
			}

			// Grab the (possibly new) RLE chunk size and
			// validate it.
			coder->rle.size = coder->options->rle;
			if (coder->rle.size > LZMA_SUBBLOCK_RLE_MAX)
				return LZMA_HEADER_ERROR;

			if (coder->subblock.size != 0
					&& coder->rle.size
						!= LZMA_SUBBLOCK_RLE_OFF
					&& coder->subblock.size
						% coder->rle.size == 0) {

				// Initialize coder->rle.buffer if we don't
				// have RLE already running.
				if (coder->rle.count == 0)
					memcpy(coder->rle.buffer,
							coder->subblock.data,
							coder->rle.size);

				// Test if coder->subblock.data is repeating.
				const size_t count = coder->subblock.size
						/ coder->rle.size;
				if (is_repeating(coder->rle.buffer,
						coder->rle.size,
						coder->subblock.data, count)) {
					if (LZMA_VLI_VALUE_MAX - count
							< coder->rle.count)
						return LZMA_PROG_ERROR;

					coder->rle.count += count;
					coder->subblock.size = 0;

				} else if (coder->rle.count > 0) {
					// It's not repeating or at least not
					// with the same byte sequence as the
					// earlier Subblock Data buffers. We
					// have some data pending in the RLE
					// buffer already, so do a flush.
					// Once flushed, we will check again
					// if the Subblock Data happens to
					// contain a different repeating
					// sequence.
					subblock_rle_flush(coder);
					break;
				}
			}
		}

		// If we now have some data left in coder->subblock, the RLE
		// buffer is empty and we must write a regular Subblock Data.
		if (coder->subblock.size > 0) {
			assert(coder->rle.count == 0);
			coder->tmp = coder->subblock.size - 1;
			coder->sequence = SEQ_DATA_SIZE_0;
			break;
		}

		// Check if we should enable Subfilter.
		if (coder->subfilter.mode == SUB_SET) {
			if (coder->rle.count > 0)
				subblock_rle_flush(coder);
			else
				coder->sequence = SEQ_SUBFILTER_INIT;
			break;
		}

		// Check if we have just finished Subfiltering.
		if (coder->subfilter.mode == SUB_END_MARKER) {
			if (coder->rle.count > 0) {
				subblock_rle_flush(coder);
				break;
			}

			write_byte(0x50);
			coder->subfilter.mode = SUB_NONE;
			if (*out_pos == out_size)
				return LZMA_OK;
		}

		// Check if we have already written everything.
		if (action == LZMA_FINISH && *in_pos == in_size
				&& coder->subfilter.mode == SUB_NONE) {
			if (coder->rle.count > 0) {
				subblock_rle_flush(coder);
				break;
			}

			if (coder->uncompressed_size
					== LZMA_VLI_VALUE_UNKNOWN) {
				// NOTE: No need to use write_byte() here
				// since we are finishing.
				out[*out_pos] = 0x10;
				++*out_pos;
			} else if (coder->uncompressed_size != 0) {
				return LZMA_DATA_ERROR;
			}

			return LZMA_STREAM_END;
		}

		// Otherwise we have more work to do.
		coder->sequence = SEQ_FILL;
		break;

	case SEQ_RLE_COUNT_0:
		// Make the Data field properly aligned, but only if the data
		// chunk to be repeated isn't extremely small. We have four
		// bytes for Count and one byte for Size, thus the number five.
		if (coder->rle.size >= RLE_MIN_SIZE_FOR_ALIGN
				&& subblock_align(
					coder, out, out_pos, out_size, 5))
			return LZMA_OK;

		assert(coder->rle.count > 0);

		write_byte(0x30 | (coder->tmp & 0x0F));

		coder->sequence = SEQ_RLE_COUNT_1;
		break;

	case SEQ_RLE_COUNT_1:
		write_byte(coder->tmp >> 4);
		coder->sequence = SEQ_RLE_COUNT_2;
		break;

	case SEQ_RLE_COUNT_2:
		write_byte(coder->tmp >> 12);
		coder->sequence = SEQ_RLE_COUNT_3;
		break;

	case SEQ_RLE_COUNT_3:
		write_byte(coder->tmp >> 20);

		if (coder->rle.count > REPEAT_COUNT_MAX)
			coder->rle.count -= REPEAT_COUNT_MAX;
		else
			coder->rle.count = 0;

		coder->sequence = SEQ_RLE_SIZE;
		break;

	case SEQ_RLE_SIZE:
		assert(coder->rle.size >= LZMA_SUBBLOCK_RLE_MIN);
		assert(coder->rle.size <= LZMA_SUBBLOCK_RLE_MAX);
		write_byte(coder->rle.size - 1);
		coder->sequence = SEQ_RLE_DATA;
		break;

	case SEQ_RLE_DATA:
		bufcpy(coder->rle.buffer, &coder->pos, coder->rle.size,
				out, out_pos, out_size);
		if (coder->pos < coder->rle.size)
			return LZMA_OK;

		coder->alignment.out_pos += coder->rle.size;

		coder->pos = 0;
		coder->sequence = SEQ_FLUSH;
		break;

	case SEQ_DATA_SIZE_0:
		// We need four bytes for the Size field.
		if (subblock_align(coder, out, out_pos, out_size, 4))
			return LZMA_OK;

		write_byte(0x20 | (coder->tmp & 0x0F));
		coder->sequence = SEQ_DATA_SIZE_1;
		break;

	case SEQ_DATA_SIZE_1:
		write_byte(coder->tmp >> 4);
		coder->sequence = SEQ_DATA_SIZE_2;
		break;

	case SEQ_DATA_SIZE_2:
		write_byte(coder->tmp >> 12);
		coder->sequence = SEQ_DATA_SIZE_3;
		break;

	case SEQ_DATA_SIZE_3:
		write_byte(coder->tmp >> 20);
		coder->sequence = SEQ_DATA;
		break;

	case SEQ_DATA:
		bufcpy(coder->subblock.data, &coder->pos,
				coder->subblock.size, out, out_pos, out_size);
		if (coder->pos < coder->subblock.size)
			return LZMA_OK;

		coder->alignment.out_pos += coder->subblock.size;

		coder->subblock.size = 0;
		coder->pos = 0;
		coder->sequence = SEQ_FLUSH;
		break;

	case SEQ_SUBFILTER_INIT: {
		assert(coder->subblock.size == 0);
		assert(coder->rle.count == 0);
		assert(coder->subfilter.mode == SUB_SET);
		assert(coder->options != NULL);

		// There must be a filter specified.
		if (coder->options->subfilter_options.id
				== LZMA_VLI_VALUE_UNKNOWN)
			return LZMA_HEADER_ERROR;

		// Initialize a raw encoder to work as a Subfilter.
		lzma_options_filter options[2];
		options[0] = coder->options->subfilter_options;
		options[1].id = LZMA_VLI_VALUE_UNKNOWN;

		return_if_error(lzma_raw_encoder_init(
				&coder->subfilter.subcoder, allocator,
				options, LZMA_VLI_VALUE_UNKNOWN, false));

		// Encode the Filter Flags field into a buffer. This should
		// never fail since we have already successfully initialized
		// the Subfilter itself. Check it still, and return
		// LZMA_PROG_ERROR instead of whatever the ret would say.
		lzma_ret ret = lzma_filter_flags_size(
				&coder->subfilter.flags_size, options);
		assert(ret == LZMA_OK);
		if (ret != LZMA_OK)
			return LZMA_PROG_ERROR;

		coder->subfilter.flags = lzma_alloc(
				coder->subfilter.flags_size, allocator);
		if (coder->subfilter.flags == NULL)
			return LZMA_MEM_ERROR;

		// Now we have a big-enough buffer. Encode the Filter Flags.
		// Like above, this should never fail.
		size_t dummy = 0;
		ret = lzma_filter_flags_encode(coder->subfilter.flags,
				&dummy, coder->subfilter.flags_size, options);
		assert(ret == LZMA_OK);
		assert(dummy == coder->subfilter.flags_size);
		if (ret != LZMA_OK || dummy != coder->subfilter.flags_size)
			return LZMA_PROG_ERROR;

		// Write a Subblock indicating a new Subfilter.
		write_byte(0x40);

		coder->options->subfilter_mode = LZMA_SUBFILTER_RUN;
		coder->subfilter.mode = SUB_RUN;
		coder->sequence = SEQ_SUBFILTER_FLAGS;
	}

	// Fall through

	case SEQ_SUBFILTER_FLAGS:
		// Copy the Filter Flags to the output stream.
		bufcpy(coder->subfilter.flags, &coder->pos,
				coder->subfilter.flags_size,
				out, out_pos, out_size);
		if (coder->pos < coder->subfilter.flags_size)
			return LZMA_OK;

		lzma_free(coder->subfilter.flags, allocator);
		coder->subfilter.flags = NULL;

		coder->pos = 0;
		coder->sequence = SEQ_FILL;
		break;

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static lzma_ret
subblock_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	if (coder->next.code == NULL)
		return subblock_buffer(coder, allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

	while (*out_pos < out_size
			&& (*in_pos < in_size || action == LZMA_FINISH)) {
		if (!coder->next_finished
				&& coder->temp.pos == coder->temp.size) {
			coder->temp.pos = 0;
			coder->temp.size = 0;

			const lzma_ret ret = coder->next.code(coder->next.coder,
					allocator, in, in_pos, in_size,
					coder->temp.buffer, &coder->temp.size,
					LZMA_BUFFER_SIZE, action);
			if (ret == LZMA_STREAM_END) {
				assert(action == LZMA_FINISH);
				coder->next_finished = true;
			} else if (coder->temp.size == 0 || ret != LZMA_OK) {
				return ret;
			}
		}

		const lzma_ret ret = subblock_buffer(coder, allocator,
				coder->temp.buffer, &coder->temp.pos,
				coder->temp.size, out, out_pos, out_size,
				coder->next_finished ? LZMA_FINISH : LZMA_RUN);
		if (ret == LZMA_STREAM_END) {
			assert(action == LZMA_FINISH);
			assert(coder->next_finished);
			return LZMA_STREAM_END;
		}

		if (ret != LZMA_OK)
			return ret;
	}

	return LZMA_OK;
}


static void
subblock_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_next_coder_end(&coder->subfilter.subcoder, allocator);
	lzma_free(coder->subblock.data, allocator);
	lzma_free(coder->subfilter.flags, allocator);
	return;
}


extern lzma_ret
lzma_subblock_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &subblock_encode;
		next->end = &subblock_encoder_end;

		next->coder->next = LZMA_NEXT_CODER_INIT;
		next->coder->subblock.data = NULL;
		next->coder->subblock.limit = 0;
		next->coder->subfilter.subcoder = LZMA_NEXT_CODER_INIT;
	} else {
		lzma_next_coder_end(&next->coder->subfilter.subcoder,
				allocator);
		lzma_free(next->coder->subfilter.flags, allocator);
	}

	next->coder->subfilter.flags = NULL;

	next->coder->next_finished = false;
	next->coder->sequence = SEQ_FILL;
	next->coder->options = filters[0].options;
	next->coder->uncompressed_size = filters[0].uncompressed_size;
	next->coder->pos = 0;

	next->coder->alignment.in_pending = 0;
	next->coder->alignment.in_pos = 0;
	next->coder->alignment.out_pos = 0;
	next->coder->subblock.size = 0;
	next->coder->rle.count = 0;
	next->coder->subfilter.mode = SUB_NONE;

	next->coder->temp.pos = 0;
	next->coder->temp.size = 0;

	// Grab some values from the options structure if it is available.
	size_t subblock_size_limit;
	if (next->coder->options != NULL) {
		if (next->coder->options->alignment
					< LZMA_SUBBLOCK_ALIGNMENT_MIN
				|| next->coder->options->alignment
					> LZMA_SUBBLOCK_ALIGNMENT_MAX) {
			subblock_encoder_end(next->coder, allocator);
			return LZMA_HEADER_ERROR;
		}
		next->coder->alignment.multiple
				= next->coder->options->alignment;
		subblock_size_limit = next->coder->options->subblock_data_size;
	} else {
		next->coder->alignment.multiple
				= LZMA_SUBBLOCK_ALIGNMENT_DEFAULT;
		subblock_size_limit = LZMA_SUBBLOCK_DATA_SIZE_DEFAULT;
	}

	return_if_error(subblock_data_size(next->coder, allocator,
				subblock_size_limit));

	return lzma_next_filter_init(
			&next->coder->next, allocator, filters + 1);
}
