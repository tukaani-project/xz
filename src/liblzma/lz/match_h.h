///////////////////////////////////////////////////////////////////////////////
//
/// \file       match_h.h
/// \brief      Header template for different match finders
//
//  Copyright (C) 1999-2006 Igor Pavlov
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

#include "lz_encoder_private.h"


//////////////////////
// Global constants //
//////////////////////

#undef LZMA_HASH_SIZE
#undef LZMA_FIX_HASH_SIZE
#undef LZMA_HASH_SIZE_C
#undef LZMA_FIX_HASH_SIZE_C

#define LZMA_HASH_SIZE(mf_name) LZMA_HASH_SIZE_C(mf_name)
#define LZMA_FIX_HASH_SIZE(mf_name) LZMA_FIX_HASH_SIZE_C(mf_name)

#define LZMA_HASH_SIZE_C(mf_name) \
	const uint32_t LZMA_ ## mf_name ## _HASH_SIZE

#define LZMA_FIX_HASH_SIZE_C(mf_name) \
	const uint32_t LZMA_ ## mf_name ## _FIX_HASH_SIZE

extern LZMA_HASH_SIZE(LZMA_MATCH_FINDER_NAME_UPPER);
extern LZMA_FIX_HASH_SIZE(LZMA_MATCH_FINDER_NAME_UPPER);


///////////////
// Functions //
///////////////

#undef LZMA_GET_MATCHES
#undef LZMA_SKIP
#undef LZMA_GET_MATCHES_C
#undef LZMA_SKIP_C

#define LZMA_GET_MATCHES(mf_name) LZMA_GET_MATCHES_C(mf_name)
#define LZMA_SKIP(mf_name) LZMA_SKIP_C(mf_name)

#define LZMA_GET_MATCHES_C(mf_name) \
	extern void lzma_ ## mf_name ## _get_matches( \
			lzma_lz_encoder *restrict lz, \
			uint32_t *restrict distances)

#define LZMA_SKIP_C(mf_name) \
	extern void lzma_ ## mf_name ## _skip( \
			lzma_lz_encoder *lz, uint32_t num)

LZMA_GET_MATCHES(LZMA_MATCH_FINDER_NAME_LOWER);

LZMA_SKIP(LZMA_MATCH_FINDER_NAME_LOWER);
