/**
 * \file        lzma.h
 * \brief       The public API of liblzma
 *
 * liblzma is a LZMA compression library with a zlib-like API.
 * liblzma is based on LZMA SDK found from http://7-zip.org/sdk.html.
 *
 * \author      Copyright (C) 1999-2006 Igor Pavlov
 * \author      Copyright (C) 2007 Lasse Collin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef LZMA_H
#define LZMA_H

/********************
 * External headers *
 ********************/

/* size_t */
#include <sys/types.h>

/* NULL */
#include <stddef.h>

/* uint8_t, uint32_t, uint64_t, UINT32_C, UINT64_C, UINT64_MAX. */
#include <inttypes.h>


/******************
 * GCC extensions *
 ******************/

/*
 * GCC extensions are used conditionally in the public API. It doesn't
 * break anything if these are sometimes enabled and sometimes not, only
 * affects warnings and optimizations.
 */
#if defined(__GNUC__) && __GNUC__ >= 3
#	ifndef lzma_attribute
#		define lzma_attribute(attr) __attribute__(attr)
#	endif
#	ifndef lzma_restrict
#		define lzma_restrict __restrict__
#	endif
#else
#	ifndef lzma_attribute
#		define lzma_attribute(attr)
#	endif
#	ifndef lzma_restrict
#		define lzma_restrict
#	endif
#endif


/**************
 * Subheaders *
 **************/

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subheaders check that this is defined. It is to prevent including
 * them directly from applications.
 */
#define LZMA_H_INTERNAL 1

/* Basic features */
#include "lzma/init.h"
#include "lzma/base.h"
#include "lzma/vli.h"
#include "lzma/filter.h"
#include "lzma/check.h"

/* Filters */
#include "lzma/copy.h"
#include "lzma/subblock.h"
#include "lzma/simple.h"
#include "lzma/delta.h"
#include "lzma/lzma.h"

/* Container formats and Metadata */
#include "lzma/block.h"
#include "lzma/index.h"
#include "lzma/extra.h"
#include "lzma/metadata.h"
#include "lzma/stream.h"
#include "lzma/alone.h"
#include "lzma/raw.h"
#include "lzma/auto.h"

/* Advanced features */
#include "lzma/info.h"
#include "lzma/alignment.h"
#include "lzma/stream_flags.h"
#include "lzma/memlimit.h"

/* Version number */
#include "lzma/version.h"

/*
 * All subheaders included. Undefine LZMA_H_INTERNAL to prevent applications
 * re-including the subheaders.
 */
#undef LZMA_H_INTERNAL

#ifdef __cplusplus
}
#endif

#endif /* ifndef LZMA_H */
