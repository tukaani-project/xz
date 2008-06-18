/**
 * \file        lzma/stream_flags.h
 * \brief       .lzma Stream Header and Stream Footer encoder and decoder
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
 * \brief       Size of Stream Header and Stream Footer
 *
 * Stream Header and Stream Footer have the same size and they are not
 * going to change even if a newer version of the .lzma file format is
 * developed in future.
 */
#define LZMA_STREAM_HEADER_SIZE 12


/**
 * Options for encoding and decoding Stream Header and Stream Footer
 */
typedef struct {
	/**
	 * Backward Size must be a multiple of four bytes. In this Stream
	 * format version Backward Size is the size of the Index field.
	 */
	lzma_vli backward_size;
#	define LZMA_BACKWARD_SIZE_MIN 4
#	define LZMA_BACKWARD_SIZE_MAX (LZMA_VLI_C(1) << 34)

	/**
	 * Type of the Check calculated from uncompressed data
	 */
	lzma_check_type check;

} lzma_stream_flags;


/**
 * \brief       Encode Stream Header
 *
 * \param       out         Beginning of the output buffer of
 *                          LZMA_STREAM_HEADER_SIZE bytes.
 * \param       options     Stream Header options to be encoded.
 *                          options->index_size is ignored and doesn't
 *                          need to be initialized.
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_PROG_ERROR: Invalid options.
 */
extern lzma_ret lzma_stream_header_encode(
		const lzma_stream_flags *options, uint8_t *out);


/**
 * \brief       Encode Stream Footer
 *
 * \param       out         Beginning of the output buffer of
 *                          LZMA_STREAM_HEADER_SIZE bytes.
 * \param       options     Stream Footer options to be encoded.
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_PROG_ERROR: Invalid options.
 */
extern lzma_ret lzma_stream_footer_encode(
		const lzma_stream_flags *options, uint8_t *out);


/**
 * \brief       Decode Stream Header
 *
 * \param       options     Stream Header options to be encoded.
 * \param       in          Beginning of the input buffer of
 *                          LZMA_STREAM_HEADER_SIZE bytes.
 *
 * options->index_size is always set to LZMA_VLI_VALUE_UNKNOWN. This is to
 * help comparing Stream Flags from Stream Header and Stream Footer with
 * lzma_stream_flags_equal().
 *
 * \return      - LZMA_OK: Decoding was successful.
 *              - LZMA_FORMAT_ERROR: Magic bytes don't match, thus the given
 *                buffer cannot be Stream Header.
 *              - LZMA_DATA_ERROR: CRC32 doesn't match, thus the header
 *                is corrupt.
 *              - LZMA_HEADER_ERROR: Unsupported options are present
 *                in the header.
 */
extern lzma_ret lzma_stream_header_decode(
		lzma_stream_flags *options, const uint8_t *in);


/**
 * \brief       Decode Stream Footer
 *
 * \param       options     Stream Header options to be encoded.
 * \param       in          Beginning of the input buffer of
 *                          LZMA_STREAM_HEADER_SIZE bytes.
 *
 * \return      - LZMA_OK: Decoding was successful.
 *              - LZMA_FORMAT_ERROR: Magic bytes don't match, thus the given
 *                buffer cannot be Stream Footer.
 *              - LZMA_DATA_ERROR: CRC32 doesn't match, thus the footer
 *                is corrupt.
 *              - LZMA_HEADER_ERROR: Unsupported options are present
 *                in the footer.
 */
extern lzma_ret lzma_stream_footer_decode(
		lzma_stream_flags *options, const uint8_t *in);


/**
 * \brief       Compare two lzma_stream_flags structures
 *
 * index_size values are compared only if both are not LZMA_VLI_VALUE_UNKNOWN.
 *
 * \return      true if both structures are considered equal; false otherwise.
 */
extern lzma_bool lzma_stream_flags_equal(
		const lzma_stream_flags *a, lzma_stream_flags *b);
