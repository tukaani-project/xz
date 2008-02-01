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
#include "delta_encoder.h"
#include "lzma_encoder.h"


struct lzma_coder_s {
	lzma_next_coder next;
	lzma_vli uncompressed_size;
};


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


static lzma_ret
raw_encode(lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	// Check that our amount of input stays in proper limits.
	if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
		if (action == LZMA_FINISH) {
			if (coder->uncompressed_size != in_size - *in_pos)
				return LZMA_PROG_ERROR;
		} else {
			if (coder->uncompressed_size < in_size - *in_pos)
				return LZMA_PROG_ERROR;
		}
	}

	const size_t in_start = *in_pos;

	const lzma_ret ret = coder->next.code(coder->next.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size, action);

	if (coder->uncompressed_size != LZMA_VLI_VALUE_UNKNOWN)
		coder->uncompressed_size -= *in_pos - in_start;

	return ret;
}


static void
raw_encoder_end(lzma_coder *coder, lzma_allocator *allocator)
{
	lzma_next_coder_end(&coder->next, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
raw_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options,
		lzma_vli uncompressed_size, bool allow_implicit)
{
	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &raw_encode;
		next->end = &raw_encoder_end;

		next->coder->next = LZMA_NEXT_CODER_INIT;
	}

	next->coder->uncompressed_size = uncompressed_size;

	// lzma_raw_coder_init() accesses get_function() via function pointer,
	// because this way linker doesn't statically link both encoder and
	// decoder functions if user needs only encoder or decoder.
	return lzma_raw_coder_init(&next->coder->next, allocator,
			options, uncompressed_size,
			&get_function, allow_implicit, true);
}


extern lzma_ret
lzma_raw_encoder_init(lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_options_filter *options,
		lzma_vli uncompressed_size, bool allow_implicit)
{
	lzma_next_coder_init(raw_encoder_init, next, allocator,
			options, uncompressed_size, allow_implicit);
}


extern LZMA_API lzma_ret
lzma_raw_encoder(lzma_stream *strm, const lzma_options_filter *options,
		lzma_vli uncompressed_size, lzma_bool allow_implicit)
{
	lzma_next_strm_init(strm, raw_encoder_init,
			options, uncompressed_size, allow_implicit);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
