///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_features.c
/// \brief      Information about features enabled at compile time
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


static lzma_mode modes[] = {
	LZMA_MODE_FAST,
	LZMA_MODE_NORMAL,
	LZMA_MODE_INVALID
};


LZMA_API const lzma_mode *const lzma_available_modes = modes;


static lzma_match_finder match_finders[] = {
#ifdef HAVE_MF_HC3
	LZMA_MF_HC3,
#endif

#ifdef HAVE_MF_HC4
	LZMA_MF_HC4,
#endif

#ifdef HAVE_MF_BT2
	LZMA_MF_BT2,
#endif

#ifdef HAVE_MF_BT3
	LZMA_MF_BT3,
#endif

#ifdef HAVE_MF_BT4
	LZMA_MF_BT4,
#endif

	LZMA_MF_INVALID
};


LZMA_API const lzma_match_finder *const lzma_available_match_finders
		= match_finders;
