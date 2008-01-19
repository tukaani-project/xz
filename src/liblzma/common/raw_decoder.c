///////////////////////////////////////////////////////////////////////////////
//
/// \file       raw_decoder.c
/// \brief      Raw decoder initialization API
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

#include "raw_decoder.h"
#include "copy_coder.h"
#include "simple_coder.h"
#include "subblock_decoder.h"
#include "subblock_decoder_helper.h"
#include "delta_decoder.h"
#include "lzma_decoder.h"
#include "metadata_decoder.h"


static lzma_init_function
get_function(lzma_vli id)
{
	switch (id) {
#ifdef HAVE_FILTER_COPY
	case LZMA_FILTER_COPY:
		return &lzma_copy_decoder_init;
#endif

#ifdef HAVE_FILTER_SUBBLOCK
	case LZMA_FILTER_SUBBLOCK:
		return &lzma_subblock_decoder_init;
#endif

#ifdef HAVE_FILTER_X86
	case LZMA_FILTER_X86:
		return &lzma_simple_x86_decoder_init;
#endif

#ifdef HAVE_FILTER_POWERPC
	case LZMA_FILTER_POWERPC:
		return &lzma_simple_powerpc_decoder_init;
#endif

#ifdef HAVE_FILTER_IA64
	case LZMA_FILTER_IA64:
		return &lzma_simple_ia64_decoder_init;
#endif

#ifdef HAVE_FILTER_ARM
	case LZMA_FILTER_ARM:
		return &lzma_simple_arm_decoder_init;
#endif

#ifdef HAVE_FILTER_ARMTHUMB
	case LZMA_FILTER_ARMTHUMB:
		return &lzma_simple_armthumb_decoder_init;
#endif

#ifdef HAVE_FILTER_SPARC
	case LZMA_FILTER_SPARC:
		return &lzma_simple_sparc_decoder_init;
#endif

#ifdef HAVE_FILTER_DELTA
	case LZMA_FILTER_DELTA:
		return &lzma_delta_decoder_init;
#endif

#ifdef HAVE_FILTER_LZMA
	case LZMA_FILTER_LZMA:
		return &lzma_lzma_decoder_init;
#endif

#ifdef HAVE_FILTER_SUBBLOCK
	case LZMA_FILTER_SUBBLOCK_HELPER:
		return &lzma_subblock_decoder_helper_init;
#endif
	}

	return NULL;
}


extern lzma_ret
lzma_raw_decoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options,
		lzma_vli uncompressed_size, bool allow_implicit)
{
	const lzma_ret ret = lzma_raw_coder_init(next, allocator,
			options, uncompressed_size, &get_function,
			allow_implicit, false);

	if (ret != LZMA_OK)
		lzma_next_coder_end(next, allocator);

	return ret;
}


extern LZMA_API lzma_ret
lzma_raw_decoder(lzma_stream *strm, const lzma_options_filter *options,
		lzma_vli uncompressed_size, lzma_bool allow_implicit)
{
	return_if_error(lzma_strm_init(strm));

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;

	const lzma_ret ret = lzma_raw_coder_init(&strm->internal->next,
			strm->allocator, options, uncompressed_size,
			&get_function, allow_implicit, false);

	if (ret != LZMA_OK)
		lzma_end(strm);

	return ret;
}
