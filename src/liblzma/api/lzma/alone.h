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
		lzma_stream *strm, const lzma_options_lzma *options);


/**
 * \brief       Initializes decoder for LZMA_Alone file
 *
 * The LZMA_Alone decoder supports LZMA_SYNC_FLUSH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 */
extern lzma_ret lzma_alone_decoder(lzma_stream *strm);
