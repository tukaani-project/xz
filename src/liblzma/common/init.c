///////////////////////////////////////////////////////////////////////////////
//
/// \file       init.c
/// \brief      Static internal initializations
///
/// The initializations have been splitted to so many small files to prevent
/// an application needing only decoder functions from statically linking
/// also the encoder functions.
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
lzma_init(void)
{
#ifdef HAVE_ENCODER
	lzma_init_encoder();
#endif

#ifdef HAVE_DECODER
	lzma_init_decoder();
#endif

	return;
}
