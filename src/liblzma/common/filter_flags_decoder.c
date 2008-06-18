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


#ifdef HAVE_FILTER_SUBBLOCK
static lzma_ret
properties_subblock(lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *props lzma_attribute((unused)),
		size_t prop_size lzma_attribute((unused)))
{
	if (prop_size != 0)
		return LZMA_HEADER_ERROR;

	options->options = lzma_alloc(
			sizeof(lzma_options_subblock), allocator);
	if (options->options == NULL)
		return LZMA_MEM_ERROR;

	((lzma_options_subblock *)(options->options))->allow_subfilters = true;
	return LZMA_OK;
}
#endif


#ifdef HAVE_FILTER_SIMPLE
static lzma_ret
properties_simple(lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *props, size_t prop_size)
{
	if (prop_size == 0)
		return LZMA_OK;

	if (prop_size != 4)
		return LZMA_HEADER_ERROR;

	lzma_options_simple *simple = lzma_alloc(
			sizeof(lzma_options_simple), allocator);
	if (simple == NULL)
		return LZMA_MEM_ERROR;

	simple->start_offset = integer_read_32(props);

	// Don't leave an options structure allocated if start_offset is zero.
	if (simple->start_offset == 0)
		lzma_free(simple, allocator);
	else
		options->options = simple;

	return LZMA_OK;
}
#endif


#ifdef HAVE_FILTER_DELTA
static lzma_ret
properties_delta(lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *props, size_t prop_size)
{
	if (prop_size != 1)
		return LZMA_HEADER_ERROR;

	options->options = lzma_alloc(sizeof(lzma_options_delta), allocator);
	if (options->options == NULL)
		return LZMA_MEM_ERROR;

	((lzma_options_delta *)(options->options))->distance
			= (uint32_t)(props[0]) + 1;

	return LZMA_OK;
}
#endif


#ifdef HAVE_FILTER_LZMA
static lzma_ret
properties_lzma(lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *props, size_t prop_size)
{
	// LZMA properties are always two bytes (at least for now).
	if (prop_size != 2)
		return LZMA_HEADER_ERROR;

	lzma_options_lzma *lzma = lzma_alloc(
			sizeof(lzma_options_lzma), allocator);
	if (lzma == NULL)
		return LZMA_MEM_ERROR;

	// Decode lc, lp, and pb.
	if (lzma_lzma_decode_properties(lzma, props[0]))
		goto error;

	// Check that reserved bits are unset.
	if (props[1] & 0xC0)
		goto error;

	// Decode the dictionary size.
	// FIXME The specification says that maximum is 4 GiB.
	if (props[1] > 36)
		goto error;
#if LZMA_DICTIONARY_SIZE_MAX != UINT32_C(1) << 30
#	error Update the if()-condition a few lines
#	error above to match LZMA_DICTIONARY_SIZE_MAX.
#endif

	lzma->dictionary_size = 2 | (props[1] & 1);
	lzma->dictionary_size <<= props[1] / 2 + 11;

	options->options = lzma;
	return LZMA_OK;

error:
	lzma_free(lzma, allocator);
	return LZMA_HEADER_ERROR;
}
#endif


extern LZMA_API lzma_ret
lzma_filter_flags_decode(
		lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	// Set the pointer to NULL so the caller can always safely free it.
	options->options = NULL;

	// Filter ID
	return_if_error(lzma_vli_decode(&options->id, NULL,
			in, in_pos, in_size));

	// Size of Properties
	lzma_vli prop_size;
	return_if_error(lzma_vli_decode(&prop_size, NULL,
			in, in_pos, in_size));

	// Check that we have enough input.
	if (prop_size > in_size - *in_pos)
		return LZMA_DATA_ERROR;

	// Determine the function to decode the properties.
	lzma_ret (*get_properties)(lzma_options_filter *options,
			lzma_allocator *allocator, const uint8_t *props,
			size_t prop_size);

	switch (options->id) {
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

	const uint8_t *props = in + *in_pos;
	*in_pos += prop_size;
	return get_properties(options, allocator, props, prop_size);
}
