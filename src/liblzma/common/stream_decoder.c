///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_decoder.c
/// \brief      Decodes .lzma Streams
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

#include "stream_common.h"
#include "check.h"
#include "stream_flags_decoder.h"
#include "block_decoder.h"
#include "metadata_decoder.h"


struct lzma_coder_s {
	enum {
		SEQ_STREAM_HEADER_CODE,
		SEQ_BLOCK_HEADER_INIT,
		SEQ_BLOCK_HEADER_CODE,
		SEQ_METADATA_CODE,
		SEQ_DATA_CODE,
		SEQ_STREAM_TAIL_INIT,
		SEQ_STREAM_TAIL_CODE,
	} sequence;

	/// Position in variable-length integers and in some other things.
	size_t pos;

	/// Block or Metadata decoder. This takes little memory and the same
	/// data structure can be used to decode every Block Header, so it's
	/// a good idea to have a separate lzma_next_coder structure for it.
	lzma_next_coder block_decoder;

	/// Block Header decoder; this is separate
	lzma_next_coder block_header_decoder;

	lzma_options_block block_options;

	/// Information about the sizes of the Blocks
	lzma_info *info;

	/// Current Block in *info
	lzma_info_iter iter;

	/// Number of bytes not yet processed from Data Blocks in the Stream.
	/// This can be LZMA_VLI_VALUE_UNKNOWN. If it is known, it is
	/// decremented while decoding and verified to match the reality.
	lzma_vli total_left;

	/// Like uncompressed_left above but for uncompressed data from
	/// Data Blocks.
	lzma_vli uncompressed_left;

	/// Stream Flags from Stream Header
	lzma_stream_flags header_flags;

	/// Stream Flags from Stream tail
	lzma_stream_flags tail_flags;

	/// Decoder for Stream Header and Stream tail. This takes very
	/// little memory and the same data structure can be used for
	/// both Header and tail, so it's a good idea to have a separate
	/// lzma_next_coder structure for it.
	lzma_next_coder flags_decoder;

	/// Temporary destination for the decoded Metadata.
	lzma_metadata metadata;

	/// Pointer to application-supplied pointer where to store the list
	/// of Extra Records from the Header Metadata Block.
	lzma_extra **header_extra;

	/// Same as above but Footer Metadata Block
	lzma_extra **footer_extra;
};


static lzma_ret
metadata_init(lzma_coder *coder, lzma_allocator *allocator)
{
	assert(coder->metadata.index == NULL);
	assert(coder->metadata.extra == NULL);

	// Single-Block Streams don't have Metadata Blocks.
	if (!coder->header_flags.is_multi)
		return LZMA_DATA_ERROR;

	coder->block_options.total_limit = LZMA_VLI_VALUE_UNKNOWN;

	// Limit the Uncompressed Size of a Metadata Block. This is to
	// prevent security issues where input file would have very huge
	// Metadata.
	//
	// FIXME: Hardcoded constant is ugly. Maybe we should provide
	// some way to specify this from the application.
	coder->block_options.uncompressed_limit = LZMA_VLI_C(1) << 23;

	lzma_info_size size_type;
	bool want_extra;

	// If we haven't decoded any Data Blocks yet, this is Header
	// Metadata Block.
	if (lzma_info_index_count_get(coder->info) == 0) {
		coder->block_options.has_backward_size = false;
		coder->block_options.handle_padding = true;
		size_type = LZMA_INFO_HEADER_METADATA;
		want_extra = coder->header_extra != NULL;
	} else {
		if (lzma_info_index_finish(coder->info))
			return LZMA_DATA_ERROR;

		coder->block_options.has_backward_size = true;
		coder->block_options.handle_padding = false;
		size_type = LZMA_INFO_FOOTER_METADATA;
		want_extra = coder->footer_extra != NULL;
	}

	coder->block_options.has_uncompressed_size_in_footer = false;
	coder->block_options.total_size = lzma_info_size_get(
			coder->info, size_type);

	coder->sequence = SEQ_METADATA_CODE;

	return lzma_metadata_decoder_init(&coder->block_decoder, allocator,
			&coder->block_options, &coder->metadata, want_extra);
}


static lzma_ret
data_init(lzma_coder *coder, lzma_allocator *allocator)
{
	return_if_error(lzma_info_iter_next(&coder->iter, allocator));

	return_if_error(lzma_info_iter_set(
			&coder->iter, LZMA_VLI_VALUE_UNKNOWN,
			coder->block_options.uncompressed_size));

	coder->block_options.total_size = coder->iter.total_size;
	coder->block_options.uncompressed_size = coder->iter.uncompressed_size;
	coder->block_options.total_limit = coder->total_left;
	coder->block_options.uncompressed_limit = coder->uncompressed_left;

	if (coder->header_flags.is_multi) {
		coder->block_options.has_uncompressed_size_in_footer = false;
		coder->block_options.has_backward_size = false;
		coder->block_options.handle_padding = true;
	} else {
		coder->block_options.has_uncompressed_size_in_footer
				= coder->iter.uncompressed_size
					== LZMA_VLI_VALUE_UNKNOWN;
		coder->block_options.has_backward_size = true;
		coder->block_options.handle_padding = false;
	}

	coder->sequence = SEQ_DATA_CODE;

	return lzma_block_decoder_init(&coder->block_decoder, allocator,
			&coder->block_options);
}


static lzma_ret
stream_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	while (*out_pos < out_size && (*in_pos < in_size
			|| coder->sequence == SEQ_DATA_CODE))
	switch (coder->sequence) {
	case SEQ_STREAM_HEADER_CODE: {
		const lzma_ret ret = coder->flags_decoder.code(
				coder->flags_decoder.coder,
				allocator, in, in_pos, in_size,
				NULL, NULL, 0, LZMA_RUN);
		if (ret != LZMA_STREAM_END)
			return ret;

		coder->sequence = SEQ_BLOCK_HEADER_INIT;

		// Detect if the Check type is supported and give appropriate
		// warning if it isn't. We don't warn every time a new Block
		// is started.
		lzma_check tmp;
		if (lzma_check_init(&tmp, coder->header_flags.check))
			return LZMA_UNSUPPORTED_CHECK;

		break;
	}

	case SEQ_BLOCK_HEADER_INIT: {
		coder->block_options.check = coder->header_flags.check;
		coder->block_options.has_crc32 = coder->header_flags.has_crc32;

		return_if_error(lzma_block_header_decoder_init(
				&coder->block_header_decoder, allocator,
				&coder->block_options));

		coder->sequence = SEQ_BLOCK_HEADER_CODE;
	}

	// Fall through

	case SEQ_BLOCK_HEADER_CODE: {
		lzma_ret ret = coder->block_header_decoder.code(
				coder->block_header_decoder.coder,
				allocator, in, in_pos, in_size,
				NULL, NULL, 0, LZMA_RUN);

		if (ret != LZMA_STREAM_END)
			return ret;

		if (coder->block_options.is_metadata)
			ret = metadata_init(coder, allocator);
		else
			ret = data_init(coder, allocator);

		if (ret != LZMA_OK)
			return ret;

		break;
	}

	case SEQ_METADATA_CODE: {
		lzma_ret ret = coder->block_decoder.code(
				coder->block_decoder.coder, allocator,
				in, in_pos, in_size, NULL, NULL, 0, LZMA_RUN);
		if (ret != LZMA_STREAM_END)
			return ret;

		const bool is_header_metadata = lzma_info_index_count_get(
				coder->info) == 0;

		if (is_header_metadata) {
			if (coder->header_extra != NULL) {
				*coder->header_extra = coder->metadata.extra;
				coder->metadata.extra = NULL;
			}

			if (lzma_info_size_set(coder->info,
					LZMA_INFO_HEADER_METADATA,
					coder->block_options.total_size)
					!= LZMA_OK)
				return LZMA_PROG_ERROR;

			coder->sequence = SEQ_BLOCK_HEADER_INIT;
		} else {
			if (coder->footer_extra != NULL) {
				*coder->footer_extra = coder->metadata.extra;
				coder->metadata.extra = NULL;
			}

			coder->sequence = SEQ_STREAM_TAIL_INIT;
		}

		assert(coder->metadata.extra == NULL);

		ret = lzma_info_metadata_set(coder->info, allocator,
				&coder->metadata, is_header_metadata, true);
		if (ret != LZMA_OK)
			return ret;

		// Intialize coder->total_size and coder->uncompressed_size
		// from Header Metadata.
		if (is_header_metadata) {
			coder->total_left = lzma_info_size_get(
					coder->info, LZMA_INFO_TOTAL);
			coder->uncompressed_left = lzma_info_size_get(
					coder->info, LZMA_INFO_UNCOMPRESSED);
		}

		break;
	}

	case SEQ_DATA_CODE: {
		lzma_ret ret = coder->block_decoder.code(
				coder->block_decoder.coder, allocator,
				in, in_pos, in_size, out, out_pos, out_size,
				action);

		if (ret != LZMA_STREAM_END)
			return ret;

		ret = lzma_info_iter_set(&coder->iter,
				coder->block_options.total_size,
				coder->block_options.uncompressed_size);
		if (ret != LZMA_OK)
			return ret;

		// These won't overflow since lzma_info_iter_set() succeeded.
		if (coder->total_left != LZMA_VLI_VALUE_UNKNOWN)
			coder->total_left -= coder->block_options.total_size;
		if (coder->uncompressed_left != LZMA_VLI_VALUE_UNKNOWN)
			coder->uncompressed_left -= coder->block_options
					.uncompressed_size;

		if (!coder->header_flags.is_multi) {
			ret = lzma_info_index_finish(coder->info);
			if (ret != LZMA_OK)
				return ret;

			coder->sequence = SEQ_STREAM_TAIL_INIT;
			break;
		}

		coder->sequence = SEQ_BLOCK_HEADER_INIT;
		break;
	}

	case SEQ_STREAM_TAIL_INIT: {
		lzma_ret ret = lzma_info_index_finish(coder->info);
		if (ret != LZMA_OK)
			return ret;

		ret = lzma_stream_tail_decoder_init(&coder->flags_decoder,
				allocator, &coder->tail_flags);
		if (ret != LZMA_OK)
			return ret;

		coder->sequence = SEQ_STREAM_TAIL_CODE;
	}

	// Fall through

	case SEQ_STREAM_TAIL_CODE: {
		const lzma_ret ret = coder->flags_decoder.code(
				coder->flags_decoder.coder, allocator,
				in, in_pos, in_size, NULL, NULL, 0, LZMA_RUN);
		if (ret != LZMA_STREAM_END)
			return ret;

		if (!lzma_stream_flags_is_equal(
				coder->header_flags, coder->tail_flags))
			return LZMA_DATA_ERROR;

		return LZMA_STREAM_END;
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
stream_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->block_decoder, allocator);
	lzma_next_coder_end(&coder->block_header_decoder, allocator);
	lzma_next_coder_end(&coder->flags_decoder, allocator);
	lzma_info_free(coder->info, allocator);
	lzma_index_free(coder->metadata.index, allocator);
	lzma_extra_free(coder->metadata.extra, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
stream_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_extra **header, lzma_extra **footer)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &stream_decode;
		next->end = &stream_decoder_end;

		next->coder->block_decoder = LZMA_NEXT_CODER_INIT;
		next->coder->block_header_decoder = LZMA_NEXT_CODER_INIT;
		next->coder->info = NULL;
		next->coder->flags_decoder = LZMA_NEXT_CODER_INIT;
		next->coder->metadata.index = NULL;
		next->coder->metadata.extra = NULL;
	} else {
		lzma_index_free(next->coder->metadata.index, allocator);
		next->coder->metadata.index = NULL;

		lzma_extra_free(next->coder->metadata.extra, allocator);
		next->coder->metadata.extra = NULL;
	}

	next->coder->info = lzma_info_init(next->coder->info, allocator);
	if (next->coder->info == NULL)
		return LZMA_MEM_ERROR;

	lzma_info_iter_begin(next->coder->info, &next->coder->iter);

	// Initialize Stream Header decoder.
	return_if_error(lzma_stream_header_decoder_init(
				&next->coder->flags_decoder, allocator,
				&next->coder->header_flags));

	// Reset the *foo_extra pointers to NULL. This way the caller knows
	// if there were no Extra Records. (We don't support appending
	// Records to Extra list.)
	if (header != NULL)
		*header = NULL;
	if (footer != NULL)
		*footer = NULL;

	// Reset some variables.
	next->coder->sequence = SEQ_STREAM_HEADER_CODE;
	next->coder->pos = 0;
	next->coder->uncompressed_left = LZMA_VLI_VALUE_UNKNOWN;
	next->coder->total_left = LZMA_VLI_VALUE_UNKNOWN;
	next->coder->header_extra = header;
	next->coder->footer_extra = footer;

	return LZMA_OK;
}


extern lzma_ret
lzma_stream_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		lzma_extra **header, lzma_extra **footer)
{
	lzma_next_coder_init(
			stream_decoder_init, next, allocator, header, footer);
}


extern LZMA_API lzma_ret
lzma_stream_decoder(lzma_stream *strm,
		lzma_extra **header, lzma_extra **footer)
{
	lzma_next_strm_init(strm, stream_decoder_init, header, footer);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	return LZMA_OK;
}
