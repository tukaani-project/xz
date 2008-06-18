///////////////////////////////////////////////////////////////////////////////
//
/// \file       block_private.h
/// \brief      Common stuff for Block encoder and decoder
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

#ifndef LZMA_BLOCK_COMMON_H
#define LZMA_BLOCK_COMMON_H

#include "common.h"


static inline bool
update_size(lzma_vli *size, lzma_vli add, lzma_vli limit)
{
	if (limit > LZMA_VLI_VALUE_MAX)
		limit = LZMA_VLI_VALUE_MAX;

	if (limit < *size || limit - *size < add)
		return true;

	*size += add;

	return false;
}


static inline bool
is_size_valid(lzma_vli size, lzma_vli reference)
{
	return reference == LZMA_VLI_VALUE_UNKNOWN || reference == size;
}

#endif
