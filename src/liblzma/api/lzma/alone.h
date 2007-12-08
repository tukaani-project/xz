/**
 * \file        lzma/alone.h
 * \brief       Handling of the legacy LZMA_Alone format
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
 * \brief       Options for files in the LZMA_Alone format
 */
typedef struct {
	/**
	 * \brief       Uncompressed Size and usage of End of Payload Marker
	 *
	 * In contrast to .lzma Blocks, LZMA_Alone format cannot have both
	 * uncompressed size field in the header and end of payload marker.
	 * If you don't know the uncompressed size beforehand, set it to
	 * LZMA_VLI_VALUE_UNKNOWN and liblzma will embed end of payload
	 * marker.
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       LZMA options
	 *
	 * The LZMA_Alone format supports only one filter: the LZMA filter.
	 *
	 * \note        There exists also an undocumented variant of the
	 *              LZMA_Alone format, which uses the x86 filter in
	 *              addition to LZMA. This format was never supported
	 *              by LZMA Utils and is not supported by liblzma either.
	 */
	lzma_options_lzma lzma;

} lzma_options_alone;


/**
 * \brief       Initializes LZMA_Alone encoder
 *
 * LZMA_Alone files have the suffix .lzma like the .lzma Stream files.
 * LZMA_Alone format supports only one filter, the LZMA filter. There is
 * no support for integrity checks like CRC32.
 *
 * Use this format if and only if you need to create files readable by
 * legacy LZMA tools.
 *
 * LZMA_Alone encoder doesn't support LZMA_SYNC_FLUSH or LZMA_FULL_FLUSH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_alone_encoder(
		lzma_stream *strm, const lzma_options_alone *options);


/**
 * \brief       Initializes decoder for LZMA_Alone file
 *
 * The LZMA_Alone decoder supports LZMA_SYNC_FLUSH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 */
extern lzma_ret lzma_alone_decoder(lzma_stream *strm);
