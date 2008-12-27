///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_encoder.c
/// \brief      Encodes .xz Blocks
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

#include "block_encoder.h"
#include "filter_encoder.h"
#include "check.h"


/// The maximum size of a single Block is limited by the maximum size of
/// a Stream, which is 2^63 - 1 bytes (i.e. LZMA_VLI_MAX). We could
/// take into account the headers etc. to determine the exact maximum size
/// of the Compressed Data field, but the complexity would give us nothing
/// useful. Instead, limit the size of Compressed Data so that even with
/// biggest possible Block Header and Check fields the total encoded size of
/// the Block stays as valid VLI. This way we don't produce incorrect output
/// if someone will really try creating a Block of 8 EiB.
///
/// ~LZMA_VLI_C(3) is to guarantee that if we need padding at the end of
/// the Compressed Data field, it will still stay in the proper limit.
#define COMPRESSED_SIZE_MAX ((LZMA_VLI_MAX - LZMA_BLOCK_HEADER_SIZE_MAX \
		- LZMA_CHECK_SIZE_MAX) & ~LZMA_VLI_C(3))


struct lzma_coder_s {
	/// The filters in the chain; initialized with lzma_raw_decoder_init().
	lzma_next_coder next;

	/// Encoding options; we also write Unpadded Size, Compressed Size,
	/// and Uncompressed Size back to this structure when the encoding
	/// has been finished.
	lzma_block *block;

	enum {
		SEQ_CODE,
		SEQ_PADDING,
		SEQ_CHECK,
	} sequence;

	/// Compressed Size calculated while encoding
	lzma_vli compressed_size;

	/// Uncompressed Size calculated while encoding
	lzma_vli uncompressed_size;

	/// Position in Block Padding and the Check fields
	size_t pos;

	/// Check of the uncompressed data
	lzma_check_state check;
};


static lzma_ret
block_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Check that our amount of input stays in proper limits.
	if (LZMA_VLI_MAX - coder->uncompressed_size < in_size - *in_pos)
		return LZMA_PROG_ERROR;

	switch (coder->sequence) {
	case SEQ_CODE: {
		const size_t in_start = *in_pos;
		const size_t out_start = *out_pos;

		const lzma_ret ret = coder->next.code(coder->next.coder,
				allocator, in, in_pos, in_size,
				out, out_pos, out_size, action);

		const size_t in_used = *in_pos - in_start;
		const size_t out_used = *out_pos - out_start;

		if (COMPRESSED_SIZE_MAX - coder->compressed_size < out_used)
			return LZMA_DATA_ERROR;

		coder->compressed_size += out_used;

		// No need to check for overflow because we have already
		// checked it at the beginning of this function.
		coder->uncompressed_size += in_used;

		lzma_check_update(&coder->check, coder->block->check,
				in + in_start, in_used);

		if (ret != LZMA_STREAM_END || action == LZMA_SYNC_FLUSH)
			return ret;

		assert(*in_pos == in_size);
		assert(action == LZMA_FINISH);

		// Copy the values into coder->block. The caller
		// may use this information to construct Index.
		coder->block->compressed_size = coder->compressed_size;
		coder->block->uncompressed_size = coder->uncompressed_size;

		coder->sequence = SEQ_PADDING;
	}

	// Fall through

	case SEQ_PADDING:
		// Pad Compressed Data to a multiple of four bytes.
		while ((coder->compressed_size + coder->pos) & 3) {
			if (*out_pos >= out_size)
				return LZMA_OK;

			out[*out_pos] = 0x00;
			++*out_pos;
			++coder->pos;
		}

		if (coder->block->check == LZMA_CHECK_NONE)
			return LZMA_STREAM_END;

		lzma_check_finish(&coder->check, coder->block->check);

		coder->pos = 0;
		coder->sequence = SEQ_CHECK;

	// Fall through

	case SEQ_CHECK: {
		const uint32_t check_size
				= lzma_check_size(coder->block->check);

		while (*out_pos < out_size) {
			out[*out_pos] = coder->check.buffer.u8[coder->pos];
			++*out_pos;

			if (++coder->pos == check_size)
				return LZMA_STREAM_END;
		}

		return LZMA_OK;
	}
	}

	return LZMA_PROG_ERROR;
}


static void
block_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_block_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_block *block)
{
	lzma_next_coder_init(lzma_block_encoder_init, next, allocator);

	if (block->version != 0)
		return LZMA_OPTIONS_ERROR;

	// If the Check ID is not supported, we cannot calculate the check and
	// thus not create a proper Block.
	if ((unsigned int)(block->check) > LZMA_CHECK_ID_MAX)
		return LZMA_PROG_ERROR;

	if (!lzma_check_is_supported(block->check))
		return LZMA_UNSUPPORTED_CHECK;

	// Allocate and initialize *next->coder if needed.
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &block_encode;
		next->end = &block_encoder_end;
		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	// Basic initializations
	next->coder->sequence = SEQ_CODE;
	next->coder->block = block;
	next->coder->compressed_size = 0;
	next->coder->uncompressed_size = 0;
	next->coder->pos = 0;

	// Initialize the check
	lzma_check_init(&next->coder->check, block->check);

	// Initialize the requested filters.
	return lzma_raw_encoder_init(&next->coder->next, allocator,
			block->filters);
}


extern LZMA_API lzma_ret
lzma_block_encoder(lzma_stream *strm, lzma_block *block)
{
	lzma_next_strm_init(lzma_block_encoder_init, strm, block);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
