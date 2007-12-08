///////////////////////////////////////////////////////////////////////////////
//
/// \file       init_decoder.c
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


extern LZMA_API void
lzma_init_decoder(void)
{
	// So far there's no decoder-specific stuff to initialize.

#ifdef HAVE_CHECK
	lzma_init_check();
#endif

	return;
}
