/**
 * \file        lzma/simple.h
 * \brief       So called "simple" filters
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


/* Filter IDs for lzma_options_filter.id */

#define LZMA_FILTER_X86         LZMA_VLI_C(0x04)
	/**<
	 * BCJ (Branch, Call, Jump) filter for x86 binaries
	 */

#define LZMA_FILTER_POWERPC     LZMA_VLI_C(0x05)
	/**<
	 * Filter for Big endian PowerPC binaries
	 */

#define LZMA_FILTER_IA64        LZMA_VLI_C(0x06)
	/**<
	 * Filter for IA64 (Itanium) binaries.
	 */

#define LZMA_FILTER_ARM         LZMA_VLI_C(0x07)
	/**<
	 * Filter for ARM binaries.
	 */

#define LZMA_FILTER_ARMTHUMB    LZMA_VLI_C(0x08)
	/**<
	 * Filter for ARMThumb binaries.
	 */

#define LZMA_FILTER_SPARC       LZMA_VLI_C(0x09)
	/**<
	 * Filter for SPARC binaries.
	 */


/**
 * \brief       Options for so called "simple" filters
 *
 * The simple filters never change the size of the data. Specifying options
 * for them is optional: if pointer to options is NULL, default values are
 * used. You probably never need to specify these options, so just set the
 * options pointer to NULL and be happy.
 *
 * If options with non-default values have been specified when encoding,
 * the same options must also be specified when decoding.
 *
 * \note        At the moment, none of the simple filters support
 *              LZMA_SYNC_FLUSH. If LZMA_SYNC_FLUSH is specified,
 *              LZMA_HEADER_ERROR will be returned. If there is need,
 *              partial support for LZMA_SYNC_FLUSH can be added in future.
 *              Partial means that flushing would be possible only at
 *              offsets that are multiple of 2, 4, or 16 depending on
 *              the filter, except x86 which cannot be made to support
 *              LZMA_SYNC_FLUSH predictably.
 */
typedef struct {
	/**
	 * \brief       Start offset for branch conversions
	 *
	 * This setting is useful only when the same filter is used
	 * _separately_ for multiple sections of the same executable file,
	 * and the sections contain cross-section branch/call/jump
	 * instructions. In that case it is benefical to set the start
	 * offset of the non-first sections so that the relative addresses
	 * of the cross-section branch/call/jump instructions will use the
	 * same absolute addresses as in the first section.
	 *
	 * When the pointer to options is NULL, the default value is used.
	 * The default value is zero.
	 */
	uint32_t start_offset;

} lzma_options_simple;
