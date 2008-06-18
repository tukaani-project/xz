/**
 * \file        lzma/easy.h
 * \brief       Easy to use encoder initialization
 *
 * \author      Copyright (C) 1999-2006 Igor Pavlov
 * \author      Copyright (C) 2008 Lasse Collin
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
 * \brief       Compression level names for lzma_easy_* functions
 *
 * At the moment, all the compression levels support LZMA_SYNC_FLUSH.
 * In future there may be levels that don't support LZMA_SYNC_FLUSH.
 * However, the LZMA_SYNC_FLUSH support won't be removed from the
 * existing compression levels.
 *
 * \note        If liblzma is built without encoder support, or with some
 *              filters disabled, some of the compression levels may be
 *              unsupported. In that case, the initialization functions
 *              will return LZMA_HEADER_ERROR.
 */
typedef enum {
	LZMA_EASY_COPY,
		/**<
		 * No compression; the data is just wrapped into .lzma
		 * container.
		 */

	LZMA_EASY_LZMA_1,
		/**<
		 * LZMA filter with fast compression (fast in terms of LZMA).
		 * If you are interested in the exact options used, see
		 * lzma_preset_lzma[0]. Note that the exact options may
		 * change between liblzma versions.
		 *
		 * At the moment, the command line tool uses these settings
		 * when `lzma -1' is used. In future, the command line tool
		 * may default to some more complex way to determine the
		 * settings used e.g. the type of files being compressed.
		 *
		 * LZMA_EASY_LZMA_2 is equivalent to lzma_preset_lzma[1]
		 * and so on.
		 */

	LZMA_EASY_LZMA_2,
	LZMA_EASY_LZMA_3,
	LZMA_EASY_LZMA_4,
	LZMA_EASY_LZMA_5,
	LZMA_EASY_LZMA_6,
	LZMA_EASY_LZMA_7,
	LZMA_EASY_LZMA_8,
	LZMA_EASY_LZMA_9,
} lzma_easy_level;


/**
 * \brief       Default compression level
 *
 * Data Blocks contain the actual compressed data. It's not straightforward
 * to recommend a default level, because in some cases keeping the resource
 * usage relatively low is more important that getting the maximum
 * compression ratio.
 */
#define LZMA_EASY_DEFAULT LZMA_EASY_LZMA_7


/**
 * \brief       Calculates rough memory requirements of a compression level
 *
 * This function is a wrapper for lzma_memory_usage(), which is declared
 * in lzma/filter.h.
 *
 * \return      Approximate memory usage of the encoder with the given
 *              compression level in mebibytes (value * 1024 * 1024 bytes).
 *              On error (e.g. compression level is not supported),
 *              UINT32_MAX is returned.
 */
extern uint32_t lzma_easy_memory_usage(lzma_easy_level level);


/**
 * \brief       Initializes .lzma Stream encoder
 *
 * This function is intended for those who just want to use the basic LZMA
 * features (that is, most developers out there). Lots of assumptions are
 * made, which are correct or at least good enough for most situations.
 *
 * \param       strm    Pointer to lzma_stream that is at least initialized
 *                      with LZMA_STREAM_INIT.
 * \param       level   Compression level to use. This selects a set of
 *                      compression settings from a list of compression
 *                      presets.
 *
 * \return      - LZMA_OK: Initialization succeeded. Use lzma_code() to
 *                encode your data.
 *              - LZMA_MEM_ERROR: Memory allocation failed. All memory
 *                previously allocated for *strm is now freed.
 *              - LZMA_HEADER_ERROR: The given compression level is not
 *                supported by this build of liblzma.
 *
 * If initialization succeeds, use lzma_code() to do the actual encoding.
 * Valid values for `action' (the second argument of lzma_code()) are
 * LZMA_RUN, LZMA_SYNC_FLUSH, LZMA_FULL_FLUSH, and LZMA_FINISH. In future,
 * there may be compression levels that don't support LZMA_SYNC_FLUSH.
 */
extern lzma_ret lzma_easy_encoder(lzma_stream *strm, lzma_easy_level level);
