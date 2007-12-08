/**
 * \file        lzma/check.h
 * \brief       Integrity checks
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
 * \brief       Type of the Check
 *
 * The .lzma format supports multiple types of Checks that are calculated
 * from the uncompressed data (unless it is empty; then it's calculated
 * from Block Header).
 */
typedef enum {
	LZMA_CHECK_NONE     = 0,
		/**<
		 * No Check is calculated.
		 *
		 * Size of the Check field: 0 bytes
		 */

	LZMA_CHECK_CRC32    = 1,
		/**<
		 * CRC32 using the polynomial from the IEEE 802.3 standard
		 *
		 * Size of the Check field: 4 bytes
		 */

	LZMA_CHECK_CRC64    = 3,
		/**<
		 * CRC64 using the polynomial from the ECMA-182 standard
		 *
		 * Size of the Check field: 8 bytes
		 */

	LZMA_CHECK_SHA256   = 5
		/**<
		 * SHA-256
		 *
		 * Size of the Check field: 32 bytes
		 */
} lzma_check_type;


/**
 * \brief       Maximum valid Check ID
 *
 * The .lzma file format specification specifies eight Check IDs (0-7). Some
 * of them are only reserved i.e. no actual Check algorithm has been assigned.
 * Still liblzma accepts any of these eight IDs for future compatibility
 * when decoding files. If a valid but unsupported Check ID is detected,
 * liblzma indicates a warning with LZMA_UNSUPPORTED_CHECK.
 *
 * FIXME bad desc
 */
#define LZMA_CHECK_ID_MAX 7


/**
 * \brief       Check IDs supported by this liblzma build
 *
 * If lzma_available_checks[n] is true, the Check ID n is supported by this
 * liblzma build. You can assume that LZMA_CHECK_NONE and LZMA_CHECK_CRC32
 * are always available.
 */
extern const lzma_bool lzma_available_checks[LZMA_CHECK_ID_MAX + 1];


/**
 * \brief       Size of the Check field with different Check IDs
 *
 * Although not all Check IDs have a check algorithm associated, the size of
 * every Check is already frozen. This array contains the size (in bytes) of
 * the Check field with specified Check ID. The values are taken from the
 * section 2.2.2 of the .lzma file format specification:
 * { 0, 4, 4, 8, 16, 32, 32, 64 }
 */
extern const uint32_t lzma_check_sizes[LZMA_CHECK_ID_MAX + 1];


/**
 * \brief       Calculate CRC32
 *
 * Calculates CRC32 using the polynomial from the IEEE 802.3 standard.
 *
 * \param       buf     Pointer to the input buffer
 * \param       size    Size of the input buffer
 * \param       crc     Previously returned CRC value. This is used to
 *                      calculate the CRC of a big buffer in smaller chunks.
 *                      Set to zero when there is no previous value.
 *
 * \return      Updated CRC value, which can be passed to this function
 *              again to continue CRC calculation.
 */
extern uint32_t lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc);


/**
 * \brief       Calculate CRC64
 *
 * Calculates CRC64 using the polynomial from the ECMA-182 standard.
 *
 * This function is used similarly to lzma_crc32(). See its documentation.
 */
extern uint64_t lzma_crc64(const uint8_t *buf, size_t size, uint64_t crc);


/*
 * SHA256 functions are currently not exported to public API.
 * Contact the author if you think it should be.
 */
