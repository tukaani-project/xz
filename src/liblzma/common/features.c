///////////////////////////////////////////////////////////////////////////////
//
/// \file       features.c
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


static const lzma_vli filters[] = {
#ifdef HAVE_FILTER_COPY
	LZMA_FILTER_COPY,
#endif

#ifdef HAVE_FILTER_SUBBLOCK
	LZMA_FILTER_SUBBLOCK,
#endif

#ifdef HAVE_FILTER_X86
	LZMA_FILTER_X86,
#endif

#ifdef HAVE_FILTER_POWERPC
	LZMA_FILTER_POWERPC,
#endif

#ifdef HAVE_FILTER_IA64
	LZMA_FILTER_IA64,
#endif

#ifdef HAVE_FILTER_ARM
	LZMA_FILTER_ARM,
#endif

#ifdef HAVE_FILTER_ARMTHUMB
	LZMA_FILTER_ARMTHUMB,
#endif

#ifdef HAVE_FILTER_SPARC
	LZMA_FILTER_SPARC,
#endif

#ifdef HAVE_FILTER_DELTA
	LZMA_FILTER_DELTA,
#endif

#ifdef HAVE_FILTER_LZMA
	LZMA_FILTER_LZMA,
#endif

	LZMA_VLI_VALUE_UNKNOWN
};


LZMA_API const lzma_vli *const lzma_available_filter_encoders = filters;

LZMA_API const lzma_vli *const lzma_available_filter_decoders = filters;
