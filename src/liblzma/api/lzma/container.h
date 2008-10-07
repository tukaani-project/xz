/**
 * \file        lzma/container.h
 * \brief       File formats
 *
 * \author      Copyright (C) 1999-2008 Igor Pavlov
 * \author      Copyright (C) 2007-2008 Lasse Collin
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


/************
 * Encoding *
 ************/

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
 *              will return LZMA_OPTIONS_ERROR.
 */
typedef enum {
	LZMA_EASY_COPY      = 0,
		/**<
		 * No compression; the data is just wrapped into .lzma
		 * container.
		 */

	LZMA_EASY_LZMA2_1   = 1,
		/**<
		 * LZMA2 filter with fast compression (fast in terms of LZMA2).
		 * If you are interested in the exact options used, see
		 * lzma_lzma_preset(1). Note that the exact options may
		 * change between liblzma versions.
		 *
		 * At the moment, the command line tool uses these settings
		 * when `lzma -1' is used. In future, the command line tool
		 * may default to some more complex way to determine the
		 * settings used e.g. the type of files being compressed.
		 *
		 * LZMA_EASY_LZMA2_2 is equivalent to lzma_lzma_preset(2)
		 * and so on.
		 */

	LZMA_EASY_LZMA2_2    = 2,
	LZMA_EASY_LZMA2_3    = 3,
	LZMA_EASY_LZMA2_4    = 4,
	LZMA_EASY_LZMA2_5    = 5,
	LZMA_EASY_LZMA2_6    = 6,
	LZMA_EASY_LZMA2_7    = 7,
	LZMA_EASY_LZMA2_8    = 8,
	LZMA_EASY_LZMA2_9    = 9,
} lzma_easy_level;


/**
 * \brief       Default compression level
 *
 * Data Blocks contain the actual compressed data. It's not straightforward
 * to recommend a default level, because in some cases keeping the resource
 * usage relatively low is more important that getting the maximum
 * compression ratio.
 */
#define LZMA_EASY_DEFAULT LZMA_EASY_LZMA2_7


/**
 * \brief       Calculates rough memory requirements of a compression level
 *
 * This function is a wrapper for lzma_memory_usage(), which is declared
 * in filter.h.
 *
 * \return      Approximate memory usage of the encoder with the given
 *              compression level in mebibytes (value * 1024 * 1024 bytes).
 *              On error (e.g. compression level is not supported),
 *              UINT32_MAX is returned.
 */
extern uint64_t lzma_easy_memory_usage(lzma_easy_level level)
		lzma_attr_pure;


/**
 * \brief       Initializes .lzma Stream encoder
 *
 * This function is intended for those who just want to use the basic features
 * if liblzma (that is, most developers out there). Lots of assumptions are
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
 *              - LZMA_OPTIONS_ERROR: The given compression level is not
 *                supported by this build of liblzma.
 *
 * If initialization succeeds, use lzma_code() to do the actual encoding.
 * Valid values for `action' (the second argument of lzma_code()) are
 * LZMA_RUN, LZMA_SYNC_FLUSH, LZMA_FULL_FLUSH, and LZMA_FINISH. In future,
 * there may be compression levels that don't support LZMA_SYNC_FLUSH.
 */
extern lzma_ret lzma_easy_encoder(lzma_stream *strm, lzma_easy_level level)
		lzma_attr_warn_unused_result;


/**
 * \brief       Initializes .lzma Stream encoder
 *
 * \param       strm    Pointer to properly prepared lzma_stream
 * \param       filters Array of filters. This must be terminated with
 *                      filters[n].id = LZMA_VLI_UNKNOWN. There must
 *                      be 1-4 filters, but there are restrictions on how
 *                      multiple filters can be combined. FIXME Tell where
 *                      to find more information.
 * \param       check   Type of the integrity check to calculate from
 *                      uncompressed data.
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_stream_encoder(lzma_stream *strm,
		const lzma_filter *filters, lzma_check check)
		lzma_attr_warn_unused_result;


/**
 * \brief       Initializes LZMA_Alone (deprecated file format) encoder
 *
 * LZMA_Alone files have the suffix .lzma like the .lzma Stream files.
 * LZMA_Alone format supports only one filter, the LZMA filter. There is
 * no support for integrity checks like CRC32.
 *
 * Use this format if and only if you need to create files readable by
 * legacy LZMA tools such as LZMA Utils 4.32.x.
 *
 * LZMA_Alone encoder doesn't support LZMA_SYNC_FLUSH or LZMA_FULL_FLUSH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_alone_encoder(
		lzma_stream *strm, const lzma_options_lzma *options)
		lzma_attr_warn_unused_result;


/************
 * Decoding *
 ************/

/**
 * This flag makes lzma_code() return LZMA_NO_CHECK if the input stream
 * being decoded has no integrity check. Note that when used with
 * lzma_auto_decoder(), all LZMA_Alone files will trigger LZMA_NO_CHECK
 * if LZMA_TELL_NO_CHECK is used.
 */
#define LZMA_TELL_NO_CHECK              UINT32_C(0x01)


/**
 * This flag makes lzma_code() return LZMA_UNSUPPORTED_CHECK if the input
 * stream has an integrity check, but the type of the integrity check is not
 * supported by this liblzma version or build. Such files can still be
 * decoded, but the integrity check cannot be verified.
 */
#define LZMA_TELL_UNSUPPORTED_CHECK     UINT32_C(0x02)


/**
 * This flag makes lzma_code() return LZMA_GET_CHECK as soon as the type
 * of the integrity check is known. The type can then be got with
 * lzma_get_check().
 */
#define LZMA_TELL_ANY_CHECK             UINT32_C(0x04)


/**
 * This flag enables decoding of concatenated files with file formats that
 * allow concatenating compressed files as is. From the formats currently
 * supported by liblzma, only the new .lzma format allows concatenated files.
 * Concatenated files are not allowed with the LZMA_Alone format.
 *
 * This flag also affects the usage of the `action' argument for lzma_code().
 * When LZMA_CONCATENATED is used, lzma_code() won't return LZMA_STREAM_END
 * unless LZMA_FINISH is used as `action'. Thus, the application has to set
 * LZMA_FINISH in the same way as it does when encoding.
 *
 * If LZMA_CONCATENATED is not used, the decoders still accept LZMA_FINISH
 * as `action' for lzma_code(), but the usage of LZMA_FINISH isn't required.
 */
#define LZMA_CONCATENATED               UINT32_C(0x08)


/**
 * \brief       Initializes decoder for .lzma Stream
 *
 * \param       strm        Pointer to properly prepared lzma_stream
 * \param       memlimit    Rough memory usage limit as bytes
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 *              - LZMA_OPTIONS_ERROR: Unsupported flags
 */
extern lzma_ret lzma_stream_decoder(
		lzma_stream *strm, uint64_t memlimit, uint32_t flags)
		lzma_attr_warn_unused_result;


/**
 * \brief       Decode .lzma Streams and LZMA_Alone files with autodetection
 *
 * Autodetects between the .lzma Stream and LZMA_Alone formats, and
 * calls lzma_stream_decoder() or lzma_alone_decoder() once the type
 * of the file has been detected.
 *
 * \param       strm        Pointer to propertily prepared lzma_stream
 * \param       memlimit    Rough memory usage limit as bytes
 * \param       flags       Bitwise-or of flags, or zero for no flags.
 *
 * \return      - LZMA_OK: Initialization was successful.
 *              - LZMA_MEM_ERROR: Cannot allocate memory.
 *              - LZMA_OPTIONS_ERROR: Unsupported flags
 */
extern lzma_ret lzma_auto_decoder(
		lzma_stream *strm, uint64_t memlimit, uint32_t flags)
		lzma_attr_warn_unused_result;


/**
 * \brief       Initializes decoder for LZMA_Alone file
 *
 * Valid `action' arguments to lzma_code() are LZMA_RUN and LZMA_FINISH.
 * There is no need to use LZMA_FINISH, but allowing it may simplify
 * certain types of applications.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 */
extern lzma_ret lzma_alone_decoder(lzma_stream *strm, uint64_t memlimit)
		lzma_attr_warn_unused_result;
