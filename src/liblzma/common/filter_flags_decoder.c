///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_flags_decoder.c
/// \brief      Decodes a Filter Flags field
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

#include "common.h"
#include "lzma_decoder.h"


struct lzma_coder_s {
	lzma_options_filter *options;

	enum {
		SEQ_MISC,
		SEQ_ID,
		SEQ_SIZE,
		SEQ_PROPERTIES,
	} sequence;

	/// \brief      Position in variable-length integers
	size_t pos;

	/// \brief      Size of Filter Properties
	lzma_vli properties_size;
};


#ifdef HAVE_FILTER_SUBBLOCK
static lzma_ret
properties_subblock(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in lzma_attribute((unused)),
		size_t *in_pos lzma_attribute((unused)),
		size_t in_size lzma_attribute((unused)))
{
	if (coder->properties_size != 0)
		return LZMA_HEADER_ERROR;

	coder->options->options = lzma_alloc(
			sizeof(lzma_options_subblock), allocator);
	if (coder->options->options == NULL)
		return LZMA_MEM_ERROR;

	((lzma_options_subblock *)(coder->options->options))
			->allow_subfilters = true;
	return LZMA_STREAM_END;
}
#endif


#ifdef HAVE_FILTER_SIMPLE
static lzma_ret
properties_simple(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	if (coder->properties_size == 0)
		return LZMA_STREAM_END;

	if (coder->properties_size != 4)
		return LZMA_HEADER_ERROR;

	lzma_options_simple *options = coder->options->options;

	if (options == NULL) {
		options = lzma_alloc(sizeof(lzma_options_simple), allocator);
		if (options == NULL)
			return LZMA_MEM_ERROR;

		options->start_offset = 0;
		coder->options->options = options;
	}

	while (coder->pos < 4) {
		if (*in_pos == in_size)
			return LZMA_OK;

		options->start_offset
				|= (uint32_t)(in[*in_pos]) << (8 * coder->pos);
		++*in_pos;
		++coder->pos;
	}

	// Don't leave an options structure allocated if start_offset is zero.
	if (options->start_offset == 0) {
		lzma_free(options, allocator);
		coder->options->options = NULL;
	}

	return LZMA_STREAM_END;
}
#endif


#ifdef HAVE_FILTER_DELTA
static lzma_ret
properties_delta(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	if (coder->properties_size != 1)
		return LZMA_HEADER_ERROR;

	if (*in_pos == in_size)
		return LZMA_OK;

	lzma_options_delta *options = lzma_alloc(
			sizeof(lzma_options_delta), allocator);
	if (options == NULL)
		return LZMA_MEM_ERROR;

	coder->options->options = options;

	options->distance = (uint32_t)(in[*in_pos]) + 1;
	++*in_pos;

	return LZMA_STREAM_END;
}
#endif


#ifdef HAVE_FILTER_LZMA
static lzma_ret
properties_lzma(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	// LZMA properties are always two bytes (at least for now).
	if (coder->properties_size != 2)
		return LZMA_HEADER_ERROR;

	assert(coder->pos < 2);

	while (*in_pos < in_size) {
		switch (coder->pos) {
		case 0:
			// Allocate the options structure.
			coder->options->options = lzma_alloc(
					sizeof(lzma_options_lzma), allocator);
			if (coder->options->options == NULL)
				return LZMA_MEM_ERROR;

			// Decode lc, lp, and pb.
			if (lzma_lzma_decode_properties(
					coder->options->options, in[*in_pos]))
				return LZMA_HEADER_ERROR;

			++*in_pos;
			++coder->pos;
			break;

		case 1: {
			lzma_options_lzma *options = coder->options->options;

			// Check that reserved bits are unset.
			if (in[*in_pos] & 0xC0)
				return LZMA_HEADER_ERROR;

			// Decode the dictionary size. See the file format
			// specification section 4.3.4.2 to understand this.
			if (in[*in_pos] == 0) {
				options->dictionary_size = 1;

			} else if (in[*in_pos] > 59) {
				// Dictionary size is over 1 GiB.
				// It's not supported at the moment.
				return LZMA_HEADER_ERROR;
#			if LZMA_DICTIONARY_SIZE_MAX != UINT32_C(1) << 30
#				error Update the if()-condition a few lines
#				error above to match LZMA_DICTIONARY_SIZE_MAX.
#			endif

			} else {
				options->dictionary_size
					= 2 | ((in[*in_pos] + 1) & 1);
				options->dictionary_size
					<<= (in[*in_pos] - 1) / 2;
			}

			++*in_pos;
			return LZMA_STREAM_END;
		}
		}
	}

	assert(coder->pos < 2);
	return LZMA_OK;
}
#endif


static lzma_ret
filter_flags_decode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out lzma_attribute((unused)),
		size_t *restrict out_pos lzma_attribute((unused)),
		size_t out_size lzma_attribute((unused)),
		lzma_action action lzma_attribute((unused)))
{
	while (*in_pos < in_size || coder->sequence == SEQ_PROPERTIES)
	switch (coder->sequence) {
	case SEQ_MISC:
		// Determine the Filter ID and Size of Filter Properties.
		if (in[*in_pos] >= 0xE0) {
			// Using External ID. Prepare the ID
			// for variable-length integer parsing.
			coder->options->id = 0;

			if (in[*in_pos] == 0xFF) {
				// Mark that Size of Filter Properties is
				// unknown, so we know later that there is
				// external Size of Filter Properties present.
				coder->properties_size
						= LZMA_VLI_VALUE_UNKNOWN;
			} else {
				// Take Size of Filter Properties from Misc.
				coder->properties_size = in[*in_pos] - 0xE0;
			}

			coder->sequence = SEQ_ID;

		} else {
			// The Filter ID is the same as Misc.
			coder->options->id = in[*in_pos];

			// The Size of Filter Properties can be calculated
			// from Misc too.
			coder->properties_size = in[*in_pos] / 0x20;

			coder->sequence = SEQ_PROPERTIES;
		}

		++*in_pos;
		break;

	case SEQ_ID: {
		const lzma_ret ret = lzma_vli_decode(&coder->options->id,
				&coder->pos, in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			return ret;

		if (coder->properties_size == LZMA_VLI_VALUE_UNKNOWN) {
			// We have also external Size of Filter
			// Properties. Prepare the size for
			// variable-length integer parsing.
			coder->properties_size = 0;
			coder->sequence = SEQ_SIZE;
		} else {
			coder->sequence = SEQ_PROPERTIES;
		}

		// Reset pos for its next job.
		coder->pos = 0;
		break;
	}

	case SEQ_SIZE: {
		const lzma_ret ret = lzma_vli_decode(&coder->properties_size,
				&coder->pos, in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			return ret;

		coder->pos = 0;
		coder->sequence = SEQ_PROPERTIES;
		break;
	}

	case SEQ_PROPERTIES: {
		lzma_ret (*get_properties)(lzma_coder *coder,
				lzma_allocator *allocator, const uint8_t *in,
				size_t *in_pos, size_t in_size);

		switch (coder->options->id) {
#ifdef HAVE_FILTER_COPY
		case LZMA_FILTER_COPY:
			return coder->properties_size > 0
					? LZMA_HEADER_ERROR : LZMA_STREAM_END;
#endif
#ifdef HAVE_FILTER_SUBBLOCK
		case LZMA_FILTER_SUBBLOCK:
			get_properties = &properties_subblock;
			break;
#endif
#ifdef HAVE_FILTER_SIMPLE
#	ifdef HAVE_FILTER_X86
		case LZMA_FILTER_X86:
#	endif
#	ifdef HAVE_FILTER_POWERPC
		case LZMA_FILTER_POWERPC:
#	endif
#	ifdef HAVE_FILTER_IA64
		case LZMA_FILTER_IA64:
#	endif
#	ifdef HAVE_FILTER_ARM
		case LZMA_FILTER_ARM:
#	endif
#	ifdef HAVE_FILTER_ARMTHUMB
		case LZMA_FILTER_ARMTHUMB:
#	endif
#	ifdef HAVE_FILTER_SPARC
		case LZMA_FILTER_SPARC:
#	endif
			get_properties = &properties_simple;
			break;
#endif
#ifdef HAVE_FILTER_DELTA
		case LZMA_FILTER_DELTA:
			get_properties = &properties_delta;
			break;
#endif
#ifdef HAVE_FILTER_LZMA
		case LZMA_FILTER_LZMA:
			get_properties = &properties_lzma;
			break;
#endif
		default:
			return LZMA_HEADER_ERROR;
		}

		return get_properties(coder, allocator, in, in_pos, in_size);
	}

	default:
		return LZMA_PROG_ERROR;
	}

	return LZMA_OK;
}


static void
filter_flags_decoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_free(coder, allocator);
	return;
}


extern lzma_ret
lzma_filter_flags_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_options_filter *options)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &filter_flags_decode;
		next->end = &filter_flags_decoder_end;
	}

	options->id = 0;
	options->options = NULL;

	next->coder->options = options;
	next->coder->sequence = SEQ_MISC;
	next->coder->pos = 0;
	next->coder->properties_size = 0;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_filter_flags_decoder(lzma_stream *strm, lzma_options_filter *options)
{
	lzma_next_strm_init(strm, lzma_filter_flags_decoder_init, options);

	strm->internal->supported_actions[LZMA_RUN] = true;

	return LZMA_OK;
}
