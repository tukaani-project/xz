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


/// \brief      Calculates the size of the Filter Properties field
///
/// This currently can return only LZMA_OK or LZMA_HEADER_ERROR, but
/// with some new filters it may return also LZMA_PROG_ERROR.
static lzma_ret
get_properties_size(uint32_t *size, const lzma_options_filter *options)
{
	lzma_ret ret = LZMA_OK;

	switch (options->id) {
#ifdef HAVE_FILTER_COPY
	case LZMA_FILTER_COPY:
		*size = 0;
		break;
#endif

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
	// Get size of Filter Properties.
	uint32_t prop_size;
	const lzma_ret ret = get_properties_size(&prop_size, options);
	if (ret != LZMA_OK)
		return ret;

	// Size of Filter ID field if it exists.
	size_t id_size;
	size_t prop_size_size;
	if (options->id < 0xE0
			&& (lzma_vli)(prop_size) == options->id / 0x20) {
		// ID and Size of Filter Properties fit into Misc.
		id_size = 0;
		prop_size_size = 0;

	} else {
		// At least Filter ID is stored using the External ID field.
		id_size = lzma_vli_size(options->id);
		if (id_size == 0)
			return LZMA_PROG_ERROR;

		if (prop_size <= 30) {
			// Size of Filter Properties fits into Misc still.
			prop_size_size = 0;
		} else {
			// The Size of Filter Properties field is used too.
			prop_size_size = lzma_vli_size(prop_size);
			if (prop_size_size == 0)
				return LZMA_PROG_ERROR;
		}
	}

	// 1 is for the Misc field.
	*size = 1 + id_size + prop_size_size + prop_size;

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
		return LZMA_BUF_ERROR;

	for (size_t i = 0; i < 4; ++i)
		out[(*out_pos)++] = options->start_offset >> (i * 8);

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
		return LZMA_BUF_ERROR;

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
		return LZMA_BUF_ERROR;

	// LZMA Properties
	if (lzma_lzma_encode_properties(options, out + *out_pos))
		return LZMA_HEADER_ERROR;

	++*out_pos;

	// Dictionary flags
	//
	// Dictionary size is encoded using six bits of
	// which one is mantissa and five are exponent.
	//
	// There are some limits that must hold to keep
	// this coding working.
#	if LZMA_DICTIONARY_SIZE_MAX > UINT32_MAX / 2
#		error LZMA_DICTIONARY_SIZE_MAX is too big.
#	endif
#	if LZMA_DICTIONARY_SIZE_MIN < 1
#		error LZMA_DICTIONARY_SIZE_MIN cannot be zero.
#	endif

	// Validate it:
	if (options->dictionary_size < LZMA_DICTIONARY_SIZE_MIN
			|| options->dictionary_size > LZMA_DICTIONARY_SIZE_MAX)
		return LZMA_HEADER_ERROR;

	if (options->dictionary_size == 1) {
		// Special case
		out[*out_pos] = 0x00;
	} else {
		// TODO This could be more elegant.
		uint32_t i = 1;
		while (((2 | ((i + 1) & 1)) << ((i - 1) / 2))
				< options->dictionary_size)
			++i;
		out[*out_pos] = i;
	}

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
	lzma_ret ret = get_properties_size(&prop_size, options);
	if (ret != LZMA_OK)
		return ret;

	// Misc, External ID, and Size of Properties
	if (options->id < 0xE0
			&& (lzma_vli)(prop_size) == options->id / 0x20) {
		// ID and Size of Filter Properties fit into Misc.
		out[*out_pos] = options->id;
		++*out_pos;

	} else if (prop_size <= 30) {
		// Size of Filter Properties fits into Misc.
		out[*out_pos] = prop_size + 0xE0;
		++*out_pos;

		// External ID is used to encode the Filter ID. If encoding
		// the VLI fails, it's because the caller has given as too
		// little output space, which it should have checked already.
		// So return LZMA_PROG_ERROR, not LZMA_BUF_ERROR.
		size_t dummy = 0;
		if (lzma_vli_encode(options->id, &dummy, 1,
				out, out_pos, out_size) != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;

	} else {
		// Nothing fits into Misc.
		out[*out_pos] = 0xFF;
		++*out_pos;

		// External ID is used to encode the Filter ID.
		size_t dummy = 0;
		if (lzma_vli_encode(options->id, &dummy, 1,
				out, out_pos, out_size) != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;

		// External Size of Filter Properties
		dummy = 0;
		if (lzma_vli_encode(prop_size, &dummy, 1,
				out, out_pos, out_size) != LZMA_STREAM_END)
			return LZMA_PROG_ERROR;
	}

	// Filter Properties
	switch (options->id) {
#ifdef HAVE_FILTER_COPY
	case LZMA_FILTER_COPY:
		assert(prop_size == 0);
		ret = options->options == NULL ? LZMA_OK : LZMA_HEADER_ERROR;
		break;
#endif

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
