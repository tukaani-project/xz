///////////////////////////////////////////////////////////////////////////////
//
/// \file       raw_encoder.c
/// \brief      Raw encoder initialization API
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

#include "raw_encoder.h"
#include "copy_coder.h"
#include "simple_coder.h"
#include "subblock_encoder.h"
#include "delta_coder.h"
#include "lzma_encoder.h"


static lzma_init_function
get_function(lzma_vli id)
{
	switch (id) {
#ifdef HAVE_FILTER_COPY
	case LZMA_FILTER_COPY:
		return &lzma_copy_encoder_init;
#endif

#ifdef HAVE_FILTER_SUBBLOCK
	case LZMA_FILTER_SUBBLOCK:
		return &lzma_subblock_encoder_init;
#endif

#ifdef HAVE_FILTER_X86
	case LZMA_FILTER_X86:
		return &lzma_simple_x86_encoder_init;
#endif

#ifdef HAVE_FILTER_POWERPC
	case LZMA_FILTER_POWERPC:
		return &lzma_simple_powerpc_encoder_init;
#endif

#ifdef HAVE_FILTER_IA64
	case LZMA_FILTER_IA64:
		return &lzma_simple_ia64_encoder_init;
#endif

#ifdef HAVE_FILTER_ARM
	case LZMA_FILTER_ARM:
		return &lzma_simple_arm_encoder_init;
#endif

#ifdef HAVE_FILTER_ARMTHUMB
	case LZMA_FILTER_ARMTHUMB:
		return &lzma_simple_armthumb_encoder_init;
#endif

#ifdef HAVE_FILTER_SPARC
	case LZMA_FILTER_SPARC:
		return &lzma_simple_sparc_encoder_init;
#endif

#ifdef HAVE_FILTER_DELTA
	case LZMA_FILTER_DELTA:
		return &lzma_delta_encoder_init;
#endif

#ifdef HAVE_FILTER_LZMA
	case LZMA_FILTER_LZMA:
		return &lzma_lzma_encoder_init;
#endif
	}

	return NULL;
}


extern lzma_ret
lzma_raw_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options,
		lzma_vli uncompressed_size, bool allow_implicit)
{
	// lzma_raw_coder_init() accesses get_function() via function pointer,
	// because this way linker doesn't statically link both encoder and
	// decoder functions if user needs only encoder or decoder.
	const lzma_ret ret = lzma_raw_coder_init(next, allocator,
			options, uncompressed_size, &get_function,
			allow_implicit, true);

	if (ret != LZMA_OK)
		lzma_next_coder_end(next, allocator);

	return ret;
}


extern LZMA_API lzma_ret
lzma_raw_encoder(lzma_stream *strm, const lzma_options_filter *options,
		lzma_vli uncompressed_size, lzma_bool allow_implicit)
{
	return_if_error(lzma_strm_init(strm));

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	const lzma_ret ret = lzma_raw_coder_init(&strm->internal->next,
			strm->allocator, options, uncompressed_size,
			&get_function, allow_implicit, true);

	if (ret != LZMA_OK)
		lzma_end(strm);

	return ret;
}
