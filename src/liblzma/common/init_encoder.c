///////////////////////////////////////////////////////////////////////////////
//
/// \file       init_encoder.c
/// \brief      Static internal initializations
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
#include "range_encoder.h"
#include "lzma_encoder.h"


extern LZMA_API void
lzma_init_encoder(void)
{
	static bool already_initialized = false;
	if (already_initialized)
		return;

	lzma_init_check();

#if defined(HAVE_SMALL) && defined(HAVE_ENCODER_LZMA)
	lzma_rc_init();
#endif

	already_initialized = true;
	return;
}
