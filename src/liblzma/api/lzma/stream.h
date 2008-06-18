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
 * \brief       Initializes .lzma Stream encoder
 *
 * \param       strm    Pointer to properly prepared lzma_stream
 * \param       filters Array of filters. This must be terminated with
 *                      filters[n].id = LZMA_VLI_VALUE_UNKNOWN. There must
 *                      be 1-4 filters, but there are restrictions on how
 *                      multiple filters can be combined. FIXME Tell where
 *                      to find more information.
 * \param       check   Type of the integrity check to calculate from
 *                      uncompressed data.
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR
 *              - LZMA_HEADER_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_stream_encoder(lzma_stream *strm,
		const lzma_options_filter *filters, lzma_check_type check);


/**
 * \brief       Initializes decoder for .lzma Stream
 *
 * \param       strm        Pointer to propertily prepared lzma_stream
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 */
extern lzma_ret lzma_stream_decoder(lzma_stream *strm);
