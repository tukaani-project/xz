/**
 * \file        lzma/raw.h
 * \brief       Raw encoder and decoder
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
 * \brief       Initializes raw encoder
 *
 * This function may be useful when implementing custom file formats.
 *
 * \param       strm    Pointer to properly prepared lzma_stream
 * \param       options Array of lzma_options_filter structures.
 *                      The end of the array must be marked with
 *                      .id = LZMA_VLI_VALUE_UNKNOWN. The minimum
 *                      number of filters is one and the maximum is four.
 *
 * The `action' with lzma_code() can be LZMA_RUN, LZMA_SYNC_FLUSH (if the
 * filter chain supports it), or LZMA_FINISH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_HEADER_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_raw_encoder(
		lzma_stream *strm, const lzma_options_filter *options);


/**
 * \brief       Initializes raw decoder
 *
 * The initialization of raw decoder goes similarly to raw encoder.
 *
 * The `action' with lzma_code() can be LZMA_RUN or LZMA_SYNC_FLUSH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_HEADER_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_raw_decoder(
		lzma_stream *strm, const lzma_options_filter *options);
