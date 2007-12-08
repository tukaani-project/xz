/**
 * \file        lzma/auto.h
 * \brief       Decoder with automatic file format detection
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
 * \brief       Decode .lzma Streams and LZMA_Alone files with autodetection
 *
 * Autodetects between the .lzma Stream and LZMA_Alone formats, and
 * calls lzma_stream_decoder_init() or lzma_alone_decoder_init() once
 * the type of the file has been detected.
 *
 * \param       strm        Pointer to propertily prepared lzma_stream
 * \param       header      Pointer to hold a pointer to Extra Records read
 *                          from the Header Metadata Block. Use NULL if
 *                          you don't care about Extra Records.
 * \param       footer      Same as header, but for Footer Metadata Block.
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 */
extern lzma_ret lzma_auto_decoder(lzma_stream *strm,
		lzma_extra **header, lzma_extra **footer);
