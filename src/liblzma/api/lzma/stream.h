/**
 * \file        lzma/stream.h
 * \brief       .lzma Stream handling
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
 * \brief       Options for .lzma Stream encoder
 */
typedef struct {
	/**
	 * \brief       Type of integrity Check
	 *
	 * The type of the integrity Check is stored into Stream Header
	 * and Stream Footer. The same Check is used for all Blocks in
	 * the Stream.
	 */
	lzma_check_type check;

	/**
	 * \brief       Precense of CRC32 of the Block Header
	 *
	 * Set this to true if CRC32 of every Block Header should be
	 * calculated and stored in the Block Header. This is recommended.
	 *
	 * This setting is stored into Stream Header and Stream Footer.
	 */
	lzma_bool has_crc32;

	/**
	 * \brief       Uncompressed Size in bytes
	 *
	 * This is somewhat advanced feature. Most users want to set this to
	 * LZMA_VLI_VALUE_UNKNOWN to indicate unknown Uncompressed Size.
	 *
	 * If the Uncompressed Size of the Stream being encoded is known,
	 * it can be stored to the beginning of the Stream. The details
	 * differ for Single-Block and Multi-Block Streams:
	 *  - With Single-Block Streams, the Uncompressed Size is stored to
	 *    the Block Header and End of Payload Marker is omitted.
	 *  - With Multi-Block Streams, the Uncompressed Size is stored to
	 *    the Header Metadata Block. The Uncompressed Size of the Blocks
	 *    will be unknown, because liblzma cannot predict how the
	 *    application is going to split the data in Blocks.
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Alignment of the beginning of the Stream
	 *
	 * Certain filters handle data in bigger chunks than single bytes.
	 * This affects two things:
	 *   - Performance: aligned memory access is usually faster.
	 *   - Further compression ratio in custom file formats: if you
	 *     encode multiple Blocks with some non-compression filter
	 *     such as LZMA_FILTER_POWERPC, it is a good idea to keep
	 *     the inter-Block alignment correct to maximize compression
	 *     ratio when all these Blocks are finally compressed as a
	 *     single step.
	 *
	 * Usually the Stream is stored into its own file, thus
	 * the alignment is usually zero.
	 */
	uint32_t alignment;

	/**
	 * \brief       Array of filters used to encode Data Blocks
	 *
	 * There can be at maximum of seven filters. The end of the array is
	 * marked with .id = LZMA_VLI_VALUE_UNKNOWN. (That's why the array
	 * has eight members.) Minimum number of filters is zero; in that
	 * case, an implicit Copy filter is used.
	 */
	lzma_options_filter filters[8];

	/**
	 * \brief       Array of filters used to encode Metadata Blocks
	 *
	 * This is like filters[] but for Metadata Blocks. If Metadata
	 * Blocks are compressed, they usually are compressed with
	 * settings that require only little memory to uncompress e.g.
	 * LZMA with 64 KiB dictionary.
	 *
	 * \todo        Recommend a preset.
	 *
	 * When liblzma sees that the Metadata Block would be very small
	 * even in uncompressed form, it is not compressed no matter
	 * what filter have been set here. This is to avoid possibly
	 * increasing the size of the Metadata Block with bad compression,
	 * and to avoid useless overhead of filters in uncompression phase.
	 */
	lzma_options_filter metadata_filters[8];

	/**
	 * \brief       Extra information in the Header Metadata Block
	 */
	const lzma_extra *header;

	/**
	 * \brief       Extra information in the Footer Metadata Block
	 *
	 * It is enough to set this pointer any time before calling
	 * lzma_code() with LZMA_FINISH as the second argument.
	 */
	const lzma_extra *footer;

} lzma_options_stream;


/**
 * \brief       Initializes Single-Block .lzma Stream encoder
 *
 * This is the function that most developers are looking for. :-) It
 * compresses using the specified options without storing any extra
 * information.
 *
 * \todo        Describe that is_metadata is ignored, maybe something else.
 */
extern lzma_ret lzma_stream_encoder_single(
		lzma_stream *strm, const lzma_options_stream *options);


/**
 * \brief       Initializes Multi-Block .lzma Stream encoder
 *
 */
extern lzma_ret lzma_stream_encoder_multi(
		lzma_stream *strm, const lzma_options_stream *options);


/**
 * \brief       Initializes decoder for .lzma Stream
 *
 * \param       strm        Pointer to propertily prepared lzma_stream
 * \param       header      Pointer to hold a pointer to Extra Records read
 *                          from the Header Metadata Block. Use NULL if
 *                          you don't care about Extra Records.
 * \param       footer      Same as header, but for Footer Metadata Block.
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 *
 * If header and/or footer are not NULL, *header and/or *footer will be
 * initially set to NULL.
 *
 * The application can detect that Header Metadata Block has been completely
 * parsed when the decoder procudes some output the first time. If *header
 * is still NULL, there was no Extra field in the Header Metadata Block (or
 * the whole Header Metadata Block wasn't present at all).
 *
 * The application can detect that Footer Metadata Block has been parsed
 * completely when lzma_code() returns LZMA_STREAM_END. If *footer is still
 * NULL, there was no Extra field in the Footer Metadata Block.
 *
 * \note        If you use lzma_memory_limiter, the Extra Records will be
 *              allocated with it, and thus remain in the lzma_memory_limiter
 *              even after they get exported to the application via *header
 *              and *footer pointers.
 */
extern lzma_ret lzma_stream_decoder(lzma_stream *strm,
		lzma_extra **header, lzma_extra **footer);
