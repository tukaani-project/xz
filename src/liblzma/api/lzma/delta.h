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
 * Filter ID of the Delta filter. This is used as lzma_filter.id.
 */
#define LZMA_FILTER_DELTA       LZMA_VLI_C(0x03)


/**
 * \brief       Type of the delta calculation
 *
 * Currently only byte-wise delta is supported. Other possible types could
 * be, for example, delta of 16/32/64-bit little/big endian integers, but
 * these are not currently planned since byte-wise delta is almost as good.
 */
typedef enum {
	LZMA_DELTA_TYPE_BYTE
} lzma_delta_type;


/**
 * \brief       Options for the Delta filter
 *
 * These options are needed by both encoder and decoder.
 */
typedef struct {
	/** For now, this must always be LZMA_DELTA_TYPE_BYTE. */
	lzma_delta_type type;

	/**
	 * \brief       Delta distance
	 *
	 * With the only currently supported type, LZMA_DELTA_TYPE_BYTE,
	 * the distance is as bytes.
	 *
	 * Examples:
	 *  - 16-bit stereo audio: distance = 4 bytes
	 *  - 24-bit RGB image data: distance = 3 bytes
	 */
	uint32_t distance;
#	define LZMA_DELTA_DISTANCE_MIN 1
#	define LZMA_DELTA_DISTANCE_MAX 256

	/**
	 * \brief       Reserved space for possible future extensions
	 *
	 * You should not touch these, because the names of these variables
	 * may change. These are and will never be used when type is
	 * LZMA_DELTA_TYPE_BYTE, so it is safe to leave these uninitialized.
	 */
	uint32_t reserved_int1;
	uint32_t reserved_int2;
	void *reserved_ptr1;
	void *reserved_ptr2;

} lzma_options_delta;
