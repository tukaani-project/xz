///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_decoder.c
/// \brief      Filter ID mapping to filter-specific functions
//
//  Copyright (C) 2008 Lasse Collin
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

#include "filter_encoder.h"
#include "filter_common.h"
#include "lzma_encoder.h"
#include "lzma2_encoder.h"
#include "subblock_encoder.h"
#include "simple_encoder.h"
#include "delta_encoder.h"


typedef struct {
	/// Initializes the filter encoder and calls lzma_next_filter_init()
	/// for filters + 1.
	lzma_init_function init;

	/// Calculates memory usage of the encoder. If the options are
	/// invalid, UINT64_MAX is returned.
	uint64_t (*memusage)(const void *options);

	/// Calculates the minimum sane size for Blocks (or other types of
	/// chunks) to which the input data can be splitted to make
	/// multithreaded encoding possible. If this is NULL, it is assumed
	/// that the encoder is fast enough with single thread.
	lzma_vli (*chunk_size)(const void *options);

	/// Tells the size of the Filter Properties field. If options are
	/// invalid, UINT32_MAX is returned. If this is NULL, props_size_fixed
	/// is used.
	lzma_ret (*props_size_get)(uint32_t *size, const void *options);
	uint32_t props_size_fixed;

	/// Encodes Filter Properties.
	///
	/// \return     - LZMA_OK: Properties encoded sucessfully.
	///             - LZMA_HEADER_ERROR: Unsupported options
	///             - LZMA_PROG_ERROR: Invalid options or not enough
	///               output space
	lzma_ret (*props_encode)(const void *options, uint8_t *out);

} lzma_filter_encoder;


static const lzma_vli ids[] = {
#ifdef HAVE_ENCODER_LZMA
	LZMA_FILTER_LZMA,
#endif

#ifdef HAVE_ENCODER_LZMA2
	LZMA_FILTER_LZMA2,
#endif

#ifdef HAVE_ENCODER_SUBBLOCK
	LZMA_FILTER_SUBBLOCK,
#endif

#ifdef HAVE_ENCODER_X86
	LZMA_FILTER_X86,
#endif

#ifdef HAVE_ENCODER_POWERPC
	LZMA_FILTER_POWERPC,
#endif

#ifdef HAVE_ENCODER_IA64
	LZMA_FILTER_IA64,
#endif

#ifdef HAVE_ENCODER_ARM
	LZMA_FILTER_ARM,
#endif

#ifdef HAVE_ENCODER_ARMTHUMB
	LZMA_FILTER_ARMTHUMB,
#endif

#ifdef HAVE_ENCODER_SPARC
	LZMA_FILTER_SPARC,
#endif

#ifdef HAVE_ENCODER_DELTA
	LZMA_FILTER_DELTA,
#endif

	LZMA_VLI_VALUE_UNKNOWN
};


// Using a pointer to avoid putting the size of the array to API/ABI.
LZMA_API const lzma_vli *const lzma_filter_encoders = ids;


// These must be in the same order as ids[].
static const lzma_filter_encoder funcs[] = {
#ifdef HAVE_ENCODER_LZMA
	{
		.init = &lzma_lzma_encoder_init,
		.memusage = &lzma_lzma_encoder_memusage,
		.chunk_size = NULL, // FIXME
		.props_size_get = NULL,
		.props_size_fixed = 5,
		.props_encode = &lzma_lzma_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_LZMA2
	{
		.init = &lzma_lzma2_encoder_init,
		.memusage = &lzma_lzma2_encoder_memusage,
		.chunk_size = NULL, // FIXME
		.props_size_get = NULL,
		.props_size_fixed = 1,
		.props_encode = &lzma_lzma2_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_SUBBLOCK
	{
		.init = &lzma_subblock_encoder_init,
// 		.memusage = &lzma_subblock_encoder_memusage,
		.chunk_size = NULL,
		.props_size_get = NULL,
		.props_size_fixed = 0,
		.props_encode = NULL,
	},
#endif
#ifdef HAVE_ENCODER_X86
	{
		.init = &lzma_simple_x86_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_POWERPC
	{
		.init = &lzma_simple_powerpc_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_IA64
	{
		.init = &lzma_simple_ia64_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_ARM
	{
		.init = &lzma_simple_arm_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_ARMTHUMB
	{
		.init = &lzma_simple_armthumb_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_SPARC
	{
		.init = &lzma_simple_sparc_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = &lzma_simple_props_size,
		.props_encode = &lzma_simple_props_encode,
	},
#endif
#ifdef HAVE_ENCODER_DELTA
	{
		.init = &lzma_delta_encoder_init,
		.memusage = NULL,
		.chunk_size = NULL,
		.props_size_get = NULL,
		.props_size_fixed = 1,
		.props_encode = &lzma_delta_props_encode,
	},
#endif
};


static const lzma_filter_encoder *
encoder_find(lzma_vli id)
{
	for (size_t i = 0; ids[i] != LZMA_VLI_VALUE_UNKNOWN; ++i)
		if (ids[i] == id)
			return funcs + i;

	return NULL;
}


extern lzma_ret
lzma_raw_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *options)
{
	return lzma_raw_coder_init(next, allocator,
			options, (lzma_filter_find)(&encoder_find), true);
}


extern LZMA_API lzma_ret
lzma_raw_encoder(lzma_stream *strm, const lzma_filter *options)
{
	lzma_next_strm_init(lzma_raw_coder_init, strm, options,
			(lzma_filter_find)(&encoder_find), true);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}


extern LZMA_API uint64_t
lzma_memusage_encoder(const lzma_filter *filters)
{
	return lzma_memusage_coder(
			(lzma_filter_find)(&encoder_find), filters);
}


extern LZMA_API lzma_vli
lzma_chunk_size(const lzma_filter *filters)
{
	uint64_t max = 0;

	for (size_t i = 0; filters[i].id != LZMA_VLI_VALUE_UNKNOWN; ++i) {
		const lzma_filter_encoder *const fe
				= encoder_find(filters[i].id);
		if (fe->chunk_size != NULL) {
			const lzma_vli size
					= fe->chunk_size(filters[i].options);
			if (size == LZMA_VLI_VALUE_UNKNOWN)
				return LZMA_VLI_VALUE_UNKNOWN;

			if (size > max)
				max = size;
		}
	}

	return max;
}


extern LZMA_API lzma_ret
lzma_properties_size(uint32_t *size, const lzma_filter *filter)
{
	const lzma_filter_encoder *const fe = encoder_find(filter->id);
	if (fe == NULL) {
		// Unknown filter - if the Filter ID is a proper VLI,
		// return LZMA_HEADER_ERROR instead of LZMA_PROG_ERROR,
		// because it's possible that we just don't have support
		// compiled in for the requested filter.
		return filter->id <= LZMA_VLI_VALUE_MAX
				? LZMA_HEADER_ERROR : LZMA_PROG_ERROR;
	}

	if (fe->props_size_get == NULL) {
		// No props_size() function, use props_size_fixed.
		*size = fe->props_size_fixed;
		return LZMA_OK;
	}

	return fe->props_size_get(size, filter->options);
}


extern LZMA_API lzma_ret
lzma_properties_encode(const lzma_filter *filter, uint8_t *props)
{
	const lzma_filter_encoder *const fe = encoder_find(filter->id);
	if (fe == NULL)
		return LZMA_PROG_ERROR;

	if (fe->props_encode == NULL)
		return LZMA_OK;

	return fe->props_encode(filter->options, props);
}
