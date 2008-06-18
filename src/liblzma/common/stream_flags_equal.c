///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_flags_equal.c
/// \brief      Compare Stream Header and Stream Footer
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

#include "common.h"


extern LZMA_API lzma_bool
lzma_stream_flags_equal(const lzma_stream_flags *a, lzma_stream_flags *b)
{
	if (a->check != b->check)
		return false;

	// Backward Sizes are compared only if they are known in both.
	if (a->backward_size != LZMA_VLI_VALUE_UNKNOWN
			&& b->backward_size != LZMA_VLI_VALUE_UNKNOWN
			&& a->backward_size != b->backward_size)
		return false;

	return true;
}
