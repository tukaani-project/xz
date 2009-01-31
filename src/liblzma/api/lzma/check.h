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
 * \brief       Type of the integrity check (Check ID)
 *
 * The .xz format supports multiple types of checks that are calculated
 * from the uncompressed data. They very in both speed and ability to
 * detect errors.
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

	LZMA_CHECK_CRC64    = 4,
		/**<
		 * CRC64 using the polynomial from the ECMA-182 standard
		 *
		 * Size of the Check field: 8 bytes
		 */

	LZMA_CHECK_SHA256   = 10
		/**<
		 * SHA-256
		 *
		 * Size of the Check field: 32 bytes
		 */
} lzma_check;


/**
 * \brief       Maximum valid Check ID
 *
 * The .xz file format specification specifies 16 Check IDs (0-15). Some
 * of them are only reserved, that is, no actual Check algorithm has been
 * assigned. When decoding, liblzma still accepts unknown Check IDs for
 * future compatibility. If a valid but unsupported Check ID is detected,
 * liblzma can indicate a warning; see the flags LZMA_TELL_NO_CHECK,
 * LZMA_TELL_UNSUPPORTED_CHECK, and LZMA_TELL_ANY_CHECK in container.h.
 */
#define LZMA_CHECK_ID_MAX 15


/**
 * \brief       Test if the given Check ID is supported
 *
 * Returns true if the given Check ID is supported by this liblzma build.
 * Otherwise false is returned. It is safe to call this with a value that
 * is not in the range [0, 15]; in that case the return value is always false.
 *
 * You can assume that LZMA_CHECK_NONE and LZMA_CHECK_CRC32 are always
 * supported (even if liblzma is built with limited features).
 */
extern LZMA_API lzma_bool lzma_check_is_supported(lzma_check check)
		lzma_attr_const;


/**
 * \brief       Get the size of the Check field with the given Check ID
 *
 * Although not all Check IDs have a check algorithm associated, the size of
 * every Check is already frozen. This function returns the size (in bytes) of
 * the Check field with the specified Check ID. The values are:
 * { 0, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 64, 64, 64 }
 *
 * If the argument is not in the range [0, 15], UINT32_MAX is returned.
 */
extern LZMA_API uint32_t lzma_check_size(lzma_check check) lzma_attr_const;


/**
 * \brief       Maximum size of a Check field
 */
#define LZMA_CHECK_SIZE_MAX 64


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
extern LZMA_API uint32_t lzma_crc32(
		const uint8_t *buf, size_t size, uint32_t crc)
		lzma_attr_pure;


/**
 * \brief       Calculate CRC64
 *
 * Calculates CRC64 using the polynomial from the ECMA-182 standard.
 *
 * This function is used similarly to lzma_crc32(). See its documentation.
 */
extern LZMA_API uint64_t lzma_crc64(
		const uint8_t *buf, size_t size, uint64_t crc)
		lzma_attr_pure;


/*
 * SHA-256 functions are currently not exported to public API.
 * Contact Lasse Collin if you think it should be.
 */


/**
 * \brief       Get the type of the integrity check
 *
 * This function can be called only immediatelly after lzma_code() has
 * returned LZMA_NO_CHECK, LZMA_UNSUPPORTED_CHECK, or LZMA_GET_CHECK.
 * Calling this function in any other situation has undefined behavior.
 */
extern LZMA_API lzma_check lzma_get_check(const lzma_stream *strm);
