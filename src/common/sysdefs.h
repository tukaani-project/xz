///////////////////////////////////////////////////////////////////////////////
//
/// \file       sysdefs.h
/// \brief      Common includes, definitions, system-specific things etc.
///
/// This file is used also by the lzma command line tool, that's why this
/// file is separate from common.h.
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

#ifndef LZMA_SYSDEFS_H
#define LZMA_SYSDEFS_H

//////////////
// Includes //
//////////////

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "lzma.h"

#include <stdlib.h>

#ifdef HAVE_STDBOOL_H
#	include <stdbool.h>
#else
#	if ! HAVE__BOOL
typedef unsigned char _Bool;
#	endif
#	define bool _Bool
#	define false 0
#	define true 1
#	define __bool_true_false_are_defined 1
#endif

#ifdef HAVE_ASSERT_H
#	include <assert.h>
#else
#	ifdef NDEBUG
#		define assert(x)
#	else
		// TODO: Pretty bad assert() macro.
#		define assert(x) (!(x) && abort())
#	endif
#endif

#ifdef HAVE_STRING_H
#	include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#	include <strings.h>
#endif

#ifdef HAVE_MEMORY_H
#	include <memory.h>
#endif


////////////
// Macros //
////////////

#ifndef HAVE_MEMCPY
#	define memcpy(dest, src, n) bcopy(src, dest, n)
#endif

#ifndef HAVE_MEMMOVE
#	define memmove(dest, src, n) bcopy(src, dest, n)
#endif

#ifdef HAVE_MEMSET
#	define memzero(s, n) memset(s, 0, n)
#else
#	define memzero(s, n) bzero(s, n)
#endif

#ifndef MIN
#	define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#	define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#endif
