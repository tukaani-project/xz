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
 * The version number is of format xyyyzzzs where
 *  - x = major
 *  - yyy = minor
 *  - zzz = revision
 *  - s indicates stability: 0 = alpha, 1 = beta, 2 = stable
 *
 * The same xyyyzzz triplet is never reused with different stability levels.
 * For example, if 5.1.0alpha has been released, there will never be 5.1.0beta
 * or 5.1.0 stable.
 *
 * \note        The version number of liblzma has nothing to with
 *              the version number of Igor Pavlov's LZMA SDK.
 */
#define LZMA_VERSION UINT32_C(49990060)


/**
 * \brief       liblzma version number as an integer
 *
 * Returns the value of LZMA_VERSION macro at the compile time of liblzma.
 * This allows the application to compare if it was built against the same,
 * older, or newer version of liblzma that is currently running.
 */
extern uint32_t lzma_version_number(void) lzma_attr_const;


/**
 * \brief       Version number of liblzma as a string
 *
 * This function may be useful if you want to display which version of
 * liblzma your application is currently using.
 */
extern const char *lzma_version_string(void) lzma_attr_const;
