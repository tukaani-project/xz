///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder_private.h
/// \brief      Private definitions for LZ encoder
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

#ifndef LZMA_LZ_ENCODER_PRIVATE_H
#define LZMA_LZ_ENCODER_PRIVATE_H

#include "lz_encoder.h"

/// Value used to indicate unused slot
#define EMPTY_HASH_VALUE 0

/// When the dictionary and hash variables need to be adjusted to prevent
/// integer overflows. Since we use uint32_t to store the offsets, half
/// of it is the biggest safe limit.
#define MAX_VAL_FOR_NORMALIZE (UINT32_MAX / 2)


struct lzma_coder_s {
	lzma_next_coder next;
	lzma_lz_encoder lz;
};

#endif
