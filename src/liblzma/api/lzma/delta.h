/**
 * \file        lzma/delta.h
 * \brief       Delta filter
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
 * \brief       Filter ID
 *
 * Filter ID of the Delta filter. This is used as lzma_options_filter.id.
 */
#define LZMA_FILTER_DELTA       LZMA_VLI_C(0x20)


/**
 * \brief       Options for the Delta filter
 *
 * These options are needed by both encoder and decoder.
 */
typedef struct {
	/**
	 * \brief       Delta distance as bytes
	 *
	 * Examples:
	 *  - 16-bit stereo audio: distance = 4 bytes
	 *  - 24-bit RGB image data: distance = 3 bytes
	 */
	uint32_t distance;
#	define LZMA_DELTA_DISTANCE_MIN 1
#	define LZMA_DELTA_DISTANCE_MAX 256

} lzma_options_delta;
