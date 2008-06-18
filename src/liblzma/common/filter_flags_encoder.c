///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_flags_encoder.c
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
#include "lzma_encoder.h"
#include "fastpos.h"


/// Calculate the size of the Filter Properties field
static lzma_ret
get_properties_size(uint32_t *size, const lzma_options_filter *options)
{
	lzma_ret ret = LZMA_OK;

	switch (options->id) {
#ifdef HAVE_FILTER_SUBBLOCK
	case LZMA_FILTER_SUBBLOCK:
		*size = 0;
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
		if (options->options == NULL || ((const lzma_options_simple *)(
				options->options))->start_offset == 0)
			*size = 0;
		else
			*size = 4;
		break;
#endif

#ifdef HAVE_FILTER_DELTA
	case LZMA_FILTER_DELTA:
		*size = 1;
		break;
#endif

#ifdef HAVE_FILTER_LZMA
	case LZMA_FILTER_LZMA:
		*size = 2;
		break;
#endif

	default:
		// Unknown filter - if the Filter ID is a proper VLI,
		// return LZMA_HEADER_ERROR instead of LZMA_PROG_ERROR,
		// because it's possible that we just don't have support
		// compiled in for the requested filter.
		ret = options->id <= LZMA_VLI_VALUE_MAX
				? LZMA_HEADER_ERROR : LZMA_PROG_ERROR;
		break;
	}

	return ret;
}


extern LZMA_API lzma_ret
lzma_filter_flags_size(uint32_t *size, const lzma_options_filter *options)
{
	// Get size of Filter Properties. This also validates the Filter ID.
	uint32_t prop_size;
	return_if_error(get_properties_size(&prop_size, options));

	// Calculate the size of the Filter ID and Size of Properties fields.
	// These cannot fail since get_properties_size() already succeeded.
	*size = lzma_vli_size(options->id) + lzma_vli_size(prop_size)
			+ prop_size;

	return LZMA_OK;
}


#ifdef HAVE_FILTER_SIMPLE
/// Encodes Filter Properties of the so called simple filters
static lzma_ret
properties_simple(uint8_t *out, size_t *out_pos, size_t out_size,
		const lzma_options_simple *options)
{
	if (options == NULL || options->start_offset == 0)
		return LZMA_OK;

	if (out_size - *out_pos < 4)
		return LZMA_PROG_ERROR;

	integer_write_32(out + *out_pos, options->start_offset);
	*out_pos += 4;

	return LZMA_OK;
}
#endif


#ifdef HAVE_FILTER_DELTA
/// Encodes Filter Properties of the Delta filter
static lzma_ret
properties_delta(uint8_t *out, size_t *out_pos, size_t out_size,
		const lzma_options_delta *options)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	// It's possible that newer liblzma versions will support larger
	// distance values.
	if (options->distance < LZMA_DELTA_DISTANCE_MIN
			|| options->distance > LZMA_DELTA_DISTANCE_MAX)
		return LZMA_HEADER_ERROR;

	if (out_size - *out_pos < 1)
		return LZMA_PROG_ERROR;

	out[*out_pos] = options->distance - LZMA_DELTA_DISTANCE_MIN;
	++*out_pos;

	return LZMA_OK;
}
#endif


#ifdef HAVE_FILTER_LZMA
/// Encodes LZMA Properties and Dictionary Flags (two bytes)
static lzma_ret
properties_lzma(uint8_t *out, size_t *out_pos, size_t out_size,
		const lzma_options_lzma *options)
{
	if (options == NULL)
		return LZMA_PROG_ERROR;

	if (out_size - *out_pos < 2)
		return LZMA_PROG_ERROR;

	// LZMA Properties
	if (lzma_lzma_encode_properties(options, out + *out_pos))
		return LZMA_HEADER_ERROR;

	++*out_pos;

	// Dictionary flags
	//
	// Dictionary size is encoded using similar encoding that is used
	// internally by LZMA.
	//
	// This won't work if dictionary size can be zero:
#	if LZMA_DICTIONARY_SIZE_MIN < 1
#		error LZMA_DICTIONARY_SIZE_MIN cannot be zero.
#	endif

	uint32_t d = options->dictionary_size;

	// Validate it:
	if (d < LZMA_DICTIONARY_SIZE_MIN || d > LZMA_DICTIONARY_SIZE_MAX)
		return LZMA_HEADER_ERROR;

	// Round up to to the next 2^n or 2^n + 2^(n - 1) depending on which
	// one is the next:
	--d;
	d |= d >> 2;
	d |= d >> 3;
	d |= d >> 4;
	d |= d >> 8;
	d |= d >> 16;
	++d;

	// Get the highest two bits using the proper encoding:
	out[*out_pos] = get_pos_slot(d) - 24;
	++*out_pos;

	return LZMA_OK;
}
#endif


extern LZMA_API lzma_ret
lzma_filter_flags_encode(uint8_t *out, size_t *out_pos, size_t out_size,
		const lzma_options_filter *options)
{
	// Minimum output is one byte (everything fits into Misc).
	// The caller should have checked that there is enough output space,
	// so we return LZMA_PROG_ERROR instead of LZMA_BUF_ERROR.
	if (*out_pos >= out_size)
		return LZMA_PROG_ERROR;

	// Get size of Filter Properties.
	uint32_t prop_size;
	return_if_error(get_properties_size(&prop_size, options));

	// Filter ID
	return_if_error(lzma_vli_encode(options->id, NULL,
			out, out_pos, out_size));

	// Size of Properties
	return_if_error(lzma_vli_encode(prop_size, NULL,
			out, out_pos, out_size));

	// Filter Properties
	lzma_ret ret;
	switch (options->id) {
#ifdef HAVE_FILTER_SUBBLOCK
	case LZMA_FILTER_SUBBLOCK:
		assert(prop_size == 0);
		ret = LZMA_OK;
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
		ret = properties_simple(out, out_pos, out_size,
				options->options);
		break;
#endif

#ifdef HAVE_FILTER_DELTA
	case LZMA_FILTER_DELTA:
		ret = properties_delta(out, out_pos, out_size,
				options->options);
		break;
#endif

#ifdef HAVE_FILTER_LZMA
	case LZMA_FILTER_LZMA:
		ret = properties_lzma(out, out_pos, out_size,
				options->options);
		break;
#endif

	default:
		assert(0);
		ret = LZMA_PROG_ERROR;
		break;
	}

	return ret;
}
