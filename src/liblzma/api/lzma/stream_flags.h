/**
 * \file        lzma/stream_flags.h
 * \brief       .lzma Stream Header and Stream tail encoder and decoder
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
 * \brief       Size of Stream Header
 *
 * Magic Bytes (6) + Stream Flags (1) + CRC32 (4)
 */
#define LZMA_STREAM_HEADER_SIZE (6 + 1 + 4)


/**
 * \brief       Size of Stream tail
 *
 * Because Stream Footer already has a defined meaning in the file format
 * specification, we use Stream tail to denote these two fields:
 * Stream Flags (1) + Magic Bytes (2)
 */
#define LZMA_STREAM_TAIL_SIZE (1 + 2)


/**
 * Options for encoding and decoding Stream Header and Stream tail
 */
typedef struct {
	/**
	 * Type of the Check calculated from uncompressed data
	 */
	lzma_check_type check;

	/**
	 * True if Block Headers have the CRC32 field. Note that the CRC32
	 * field is always present in the Stream Header.
	 */
	lzma_bool has_crc32;

	/**
	 * True if the Stream is a Multi-Block Stream.
	 */
	lzma_bool is_multi;

} lzma_stream_flags;


#define lzma_stream_flags_is_equal(a, b) \
	((a).check == (b).check \
		&& (a).has_crc32 == (b).has_crc32 \
		&& (a).is_multi == (b).is_multi)


/**
 * \brief       Encodes Stream Header
 *
 * Encoding of the Stream Header is done with a single call instead of
 * first initializing and then doing the actual work with lzma_code().
 *
 * \param       out         Beginning of the output buffer
 * \param       out_pos     out[*out_pos] is the next write position. This
 *                          is updated by the encoder.
 * \param       out_size    out[out_size] is the first byte to not write.
 * \param       options     Stream Header options to be encoded.
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_PROG_ERROR: Invalid options.
 *              - LZMA_BUF_ERROR: Not enough output buffer space.
 */
extern lzma_ret lzma_stream_header_encode(
		uint8_t *out, const lzma_stream_flags *options);


/**
 * \brief       Encodes Stream tail
 *
 * \param       footer      Pointer to a pointer that will hold the
 *                          allocated buffer. Caller must free it once
 *                          it isn't needed anymore.
 * \param       footer_size Pointer to a variable that will the final size
 *                          of the footer buffer.
 * \param       allocator   lzma_allocator for custom allocator functions.
 *                          Set to NULL to use malloc().
 * \param       options     Stream Header options to be encoded.
 *
 * \return      - LZMA_OK: Success; *header and *header_size set.
 *              - LZMA_PROG_ERROR: *options is invalid.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 */
extern lzma_ret lzma_stream_tail_encode(
		uint8_t *out, const lzma_stream_flags *options);


/**
 * \brief       Initializes Stream Header decoder
 *
 * \param       strm        Pointer to lzma_stream used to pass input data
 * \param       options     Target structure for parsed results
 *
 * \return      - LZMA_OK: Successfully initialized
 *              - LZMA_MEM_ERROR: Cannot allocate memory
 *
 * The actual decoding is done with lzma_code() and freed with lzma_end().
 */
extern lzma_ret lzma_stream_header_decoder(
		lzma_stream *strm, lzma_stream_flags *options);


/**
 * \brief       Initializes Stream tail decoder
 *
 * \param       strm        Pointer to lzma_stream used to pass input data
 * \param       options     Target structure for parsed results.
 * \param       decode_uncompressed_size
 *                          Set to true if the first field to decode is
 *                          Uncompressed Size. Set to false if the first
 *                          field to decode is Backward Size.
 *
 * \return      - LZMA_OK: Successfully initialized
 *              - LZMA_MEM_ERROR: Cannot allocate memory
 *
 * The actual decoding is done with lzma_code() and freed with lzma_end().
 */
extern lzma_ret lzma_stream_tail_decoder(
		lzma_stream *strm, lzma_stream_flags *options);
