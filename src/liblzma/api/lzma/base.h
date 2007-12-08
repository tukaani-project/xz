/**
 * \file        lzma/base.h
 * \brief       Data types and functions used in many places of the public API
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
 * \brief       Boolean
 *
 * This is here because C89 doesn't have stdbool.h. To set a value for
 * variables having type lzma_bool, you can use
 *   - C99's `true' and `false' from stdbool.h;
 *   - C++'s internal `true' and `false'; or
 *   - integers one (true) and zero (false).
 */
typedef unsigned char lzma_bool;


/**
 * \brief       Return values used by several functions in liblzma
 *
 * Check the descriptions of specific functions to find out which return
 * values they can return and the exact meanings of the values in every
 * situation. The descriptions given here are only suggestive.
 */
typedef enum {
	LZMA_OK                 =  0,
		/**<
		 * \brief       Operation completed successfully
		 */

	LZMA_STREAM_END         =  1,
		/**<
		 * \brief       End of stream was reached
		 *
		 * The application should pick the last remaining output
		 * bytes from strm->next_out.
		 */

	LZMA_PROG_ERROR       = -2,
		/**<
		 * \brief       Programming error
		 *
		 * This indicates that the arguments given to the function are
		 * invalid or the internal state of the decoder is corrupt.
		 *   - Function arguments are invalid or the structures
		 *     pointed by the argument pointers are invalid
		 *     e.g. if strm->next_out has been set to NULL and
		 *     strm->avail_out > 0 when calling lzma_code().
		 *   - lzma_* functions have been called in wrong order
		 *     e.g. lzma_code() was called right after lzma_end().
		 *   - If errors occur randomly, the reason might be flaky
		 *     hardware.
		 *
		 * If you think that your code is correct, this error code
		 * can be a sign of a bug in liblzma. See the documentation
		 * how to report bugs.
		 */

	LZMA_DATA_ERROR         = -3,
		/**<
		 * \brief       Data is corrupt
		 *
		 * - Encoder: The input size doesn't match the uncompressed
		 *   size given to lzma_*_encoder_init().
		 * - Decoder: The input is corrupt. This includes corrupted
		 *   header, corrupted compressed data, and unmatching
		 *   integrity Check.
		 *
		 * \todo        What can be done if encoder returns this?
		 *              Probably can continue by fixing the input
		 *              amount, but make sure.
		 */

	LZMA_MEM_ERROR          = -4,
		/**<
		 * \brief       Cannot allocate memory
		 *
		 * Memory allocation failed.
		 */

	LZMA_BUF_ERROR          = -5,
		/**<
		 * \brief       No progress is possible
		 *
		 * This may happen when avail_in or avail_out is zero.
		 *
		 * \note        This error is not fatal. Coding can continue
		 *              normally once the reason for this error has
		 *              been fixed.
		 */

	LZMA_HEADER_ERROR       = -6,
		/**<
		 * \brief       Invalid or unsupported header
		 *
		 * Invalid or unsupported options, for example
		 *  - unsupported filter(s) or filter options; or
		 *  - reserved bits set in headers (decoder only).
		 *
		 * Rebuilding liblzma with more features enabled, or
		 * upgrading to a newer version of liblzma may help.
		 */

	LZMA_UNSUPPORTED_CHECK  = -7,
		/**<
		 * \brief       Check type is unknown
		 *
		 * The type of Check is not supported, and thus the Check
		 * cannot be calculated. In the encoder, this is an error.
		 * In the decoder, this is only a warning and decoding can
		 * still proceed normally (but the Check is ignored).
		 */
} lzma_ret;


/**
 * \brief       The `action' argument for lzma_code()
 */
typedef enum {
	LZMA_RUN = 0,
		/**<
		 * Encoder: Encode as much input as possible. Some internal
		 * buffering will probably be done (depends on the filter
		 * chain in use), which causes latency: the input used won't
		 * usually be decodeable from the output of the same
		 * lzma_code() call.
		 *
		 * Decoder: Decode as much input as possible and produce as
		 * much output as possible. This action provides best
		 * throughput, but may introduce latency, because the
		 * decoder may decode more data into its internal buffers
		 * than that fits into next_out.
		 */

	LZMA_SYNC_FLUSH = 1,
		/**<
		 * Encoder: Makes all the data given to liblzma via next_in
		 * available in next_out without resetting the filters. Call
		 * lzma_code() with LZMA_SYNC_FLUSH until it returns
		 * LZMA_STREAM_END. Then continue encoding normally.
		 *
		 * \note        Synchronous flushing is supported only by
		 *              some filters. Some filters support it only
		 *              partially.
		 *
		 * Decoder: Asks the decoder to decode only as much as is
		 * needed to fill next_out. This decreases latency with some
		 * filters, but is likely to decrease also throughput. It is
		 * a good idea to use this flag only when it is likely that
		 * you don't need more output soon.
		 *
		 * \note        With decoder, this is not comparable to
		 *              zlib's Z_SYNC_FLUSH.
		 */

	LZMA_FULL_FLUSH = 2,
		/**<
		 * Finishes encoding of the current Data Block. All the input
		 * data going to the current Data Block must have been given
		 * to the encoder (the last bytes can still be pending in
		 * next_in). Call lzma_code() with LZMA_FULL_FLUSH until
		 * it returns LZMA_STREAM_END. Then continue normally with
		 * LZMA_RUN or finish the Stream with LZMA_FINISH.
		 *
		 * This action is supported only by Multi-Block Stream
		 * encoder. If there is no unfinished Data Block, no empty
		 * Data Block is created.
		 */

	LZMA_FINISH = 3
		/**<
		 * Finishes the encoding operation. All the input data must
		 * have been given to the encoder (the last bytes can still
		 * be pending in next_in). Call lzma_code() with LZMA_FINISH
		 * until it returns LZMA_STREAM_END.
		 *
		 * This action is not supported by decoders.
		 */
} lzma_action;


/**
 * \brief       Custom functions for memory handling
 *
 * A pointer to lzma_allocator may be passed via lzma_stream structure
 * to liblzma. The library will use these functions for memory handling
 * instead of the default malloc() and free().
 *
 * liblzma doesn't make an internal copy of lzma_allocator. Thus, it is
 * OK to change these function pointers in the middle of the coding
 * process, but obviously it must be done carefully to make sure that the
 * replacement `free' can deallocate memory allocated by the earlier
 * `alloc' function(s).
 */
typedef struct {
	/**
	 * \brief       Pointer to custom memory allocation function
	 *
	 * Set this to point to your custom memory allocation function.
	 * It can be useful for example if you want to limit how much
	 * memory liblzma is allowed to use: for this, you may use
	 * a pointer to lzma_memory_alloc().
	 *
	 * If you don't want a custom allocator, but still want
	 * custom free(), set this to NULL and liblzma will use
	 * the standard malloc().
	 *
	 * \param       opaque  lzma_allocator.opaque (see below)
	 * \param       nmemb   Number of elements like in calloc().
	 *                      liblzma will always set nmemb to 1.
	 *                      This argument exists only for
	 *                      compatibility with zlib and libbzip2.
	 * \param       size    Size of an element in bytes.
	 *                      liblzma never sets this to zero.
	 *
	 * \return      Pointer to the beginning of a memory block of
	 *              size nmemb * size, or NULL if allocation fails
	 *              for some reason. When allocation fails, functions
	 *              of liblzma return LZMA_MEM_ERROR.
	 */
	void *(*alloc)(void *opaque, size_t nmemb, size_t size);

	/**
	 * \brief       Pointer to custom memory freeing function
	 *
	 * Set this to point to your custom memory freeing function.
	 * If lzma_memory_alloc() is used as allocator, this should
	 * be set to lzma_memory_free().
	 *
	 * If you don't want a custom freeing function, but still
	 * want a custom allocator, set this to NULL and liblzma
	 * will use the standard free().
	 *
	 * \param       opaque  lzma_allocator.opaque (see below)
	 * \param       ptr     Pointer returned by
	 *                      lzma_allocator.alloc(), or when it
	 *                      is set to NULL, a pointer returned
	 *                      by the standard malloc().
	 */
	void (*free)(void *opaque, void *ptr);

	/**
	 * \brief       Pointer passed to .alloc() and .free()
	 *
	 * opaque is passed as the first argument to lzma_allocator.alloc()
	 * and lzma_allocator.free(). This intended to ease implementing
	 * custom memory allocation functions for use with liblzma.
	 *
	 * When using lzma_memory_alloc() and lzma_memory_free(), opaque
	 * must point to lzma_memory_limitter structure allocated and
	 * initialized with lzma_memory_limitter_create().
	 *
	 * If you don't need this, you should set it to NULL.
	 */
	void *opaque;

} lzma_allocator;


/**
 * \brief       Internal data structure
 *
 * The contents of this structure is not visible outside the library.
 */
typedef struct lzma_internal_s lzma_internal;


/**
 * \brief       Passing data to and from liblzma
 *
 * The lzma_stream structure is used for
 *   - passing pointers to input and output buffers to liblzma;
 *   - defining custom memory hander functions; and
 *   - holding a pointer to coder-specific internal data structures.
 *
 * Before calling any of the lzma_*_init() functions the first time,
 * the application must reset lzma_stream to LZMA_STREAM_INIT. The
 * lzma_*_init() function will verify the options, allocate internal
 * data structures and store pointer to them into `internal'. Finally
 * total_in and total_out are reset to zero. In contrast to zlib,
 * next_in and avail_in are ignored by the initialization functions.
 *
 * The actual coding is done with the lzma_code() function. Application
 * must update next_in, avail_in, next_out, and avail_out between
 * calls to lzma_decode() just like with zlib.
 *
 * In contrast to zlib, even the decoder requires that there always
 * is at least one byte space in next_out; if avail_out == 0,
 * LZMA_BUF_ERROR is returned immediatelly. This shouldn't be a problem
 * for most applications that already use zlib, but it's still worth
 * checking your application.
 *
 * Application may modify values of total_in and total_out as it wants.
 * They are updated by liblzma to match the amount of data read and
 * written, but liblzma doesn't use the values internally.
 *
 * Application must not touch the `internal' pointer.
 */
typedef struct {
	uint8_t *next_in;   /**< Pointer to the next input byte. */
	size_t avail_in;    /**< Number of available input bytes in next_in. */
	uint64_t total_in;  /**< Total number of bytes read by liblzma. */

	uint8_t *next_out;  /**< Pointer to the next output position. */
	size_t avail_out;   /**< Amount of free space in next_out. */
	uint64_t total_out; /**< Total number of bytes written by liblzma. */

	/**
	 * Custom memory allocation functions. Set to NULL to use
	 * the standard malloc() and free().
	 */
	lzma_allocator *allocator;

	/** Internal state is not visible to outsiders. */
	lzma_internal *internal;

} lzma_stream;


/**
 * \brief       Initialization for lzma_stream
 *
 * When you declare an instance of lzma_stream, you can immediatelly
 * initialize it so that initialization functions know that no memory
 * has been allocated yet:
 *
 *     lzma_stream strm = LZMA_STREAM_INIT;
 */
#define LZMA_STREAM_INIT { NULL, 0, 0, NULL, 0, 0, NULL, NULL }


/**
 * \brief       Initialization for lzma_stream
 *
 * This is like LZMA_STREAM_INIT, but this can be used when the lzma_stream
 * has already been allocated:
 *
 *     lzma_stream *strm = malloc(sizeof(lzma_stream));
 *     if (strm == NULL)
 *         return LZMA_MEM_ERROR;
 *     *strm = LZMA_STREAM_INIT_VAR;
 */
extern const lzma_stream LZMA_STREAM_INIT_VAR;


/**
 * \brief       Encodes or decodes data
 *
 * Once the lzma_stream has been successfully initialized (e.g. with
 * lzma_stream_encoder_single()), the actual encoding or decoding is
 * done using this function.
 *
 * \return      Some coders may have more exact meaning for different return
 *              values, which are mentioned separately in the description of
 *              the initialization functions. Here are the typical meanings:
 *              - LZMA_OK: So far all good.
 *              - LZMA_STREAM_END:
 *                  - Encoder: LZMA_SYNC_FLUSH, LZMA_FULL_FLUSH, or
 *                    LZMA_FINISH completed.
 *                  - Decoder: End of uncompressed data was reached.
 *              - LZMA_BUF_ERROR: Unable to progress. Provide more input or
 *                output space, and call this function again. This cannot
 *                occur if both avail_in and avail_out were non-zero (or
 *                there's a bug in liblzma).
 *              - LZMA_MEM_ERROR: Unable to allocate memory. Due to lazy
 *                programming, the coding cannot continue even if the
 *                application could free more memory. The next call must
 *                be lzma_end() or some initialization function.
 *              - LZMA_DATA_ERROR:
 *                  - Encoder: Filter(s) cannot process the given data.
 *                  - Decoder: Compressed data is corrupt.
 *              - LZMA_HEADER_ERROR: Unsupported options. Rebuilding liblzma
 *                with more features enabled or upgrading to a newer version
 *                may help, although usually this is a sign of invalid options
 *                (encoder) or corrupted input data (decoder).
 *              - LZMA_PROG_ERROR: Invalid arguments or the internal state
 *                of the coder is corrupt.
 */
extern lzma_ret lzma_code(lzma_stream *strm, lzma_action action);


/**
 * \brief       Frees memory allocated for the coder data structures
 *
 * \param       strm    Pointer to lzma_stream that is at least initialized
 *                      with LZMA_STREAM_INIT.
 *
 * \note        zlib indicates an error if application end()s unfinished
 *              stream. liblzma doesn't do this, and assumes that
 *              application knows what it is doing.
 */
extern void lzma_end(lzma_stream *strm);
