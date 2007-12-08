/**
 * \file        lzma/version.h
 * \brief       Version number
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

#ifndef LZMA_H_INTERNAL
#	error Never include this file directly. Use <lzma.h> instead.
#endif


/**
 * \brief       Compile-time version number
 *
 * The version number is of format xyyyuuus where
 *  - x is the major LZMA SDK version
 *  - yyy is the minor LZMA SDK version
 *  - uuu is LZMA Utils version (reset to zero every time SDK version
 *    is incremented)
 *  - s indicates stability: 0 = alpha, 1 = beta, 2 = stable
 */
#define LZMA_VERSION UINT32_C(40420010)


/**
 * \brief       liblzma version number as an integer
 *
 * This is the value of LZMA_VERSION macro at the compile time of liblzma.
 * This allows the application to compare if it was built against the same,
 * older, or newer version of liblzma that is currently running.
 */
extern const uint32_t lzma_version_number;


/**
 * \brief       Returns versions number of liblzma as a string
 *
 * This function may be useful if you want to display which version of
 * libilzma your application is currently using.
 *
 * \return      Returns a pointer to a statically allocated string constant,
 *              which contains the version number of liblzma. The format of
 *              the version string is usually (but not necessarily) x.y.z
 *              e.g. "4.42.1". Alpha and beta versions contain a suffix
 *              ("4.42.0alpha").
 */
extern const char *const lzma_version_string;
