/**
 * \file        lzma/init.h
 * \brief       Initializations
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
 * \brief       Initialize all internal static variables
 *
 * Depending on the build options, liblzma may have some internal static
 * variables, that must be initialized before using any other part of
 * the library (*). It is recommended to do these initializations in the very
 * beginning of the application by calling appropriate initialization function.
 *
 * (*) There are some exceptions to this rule. FIXME
 *
 * The initialization functions are not necessarily thread-safe, thus the
 * required initializations must be done before creating any threads. (The
 * rest of the functions of liblzma are thread-safe.) Calling the
 * initialization functions multiple times does no harm, although it
 * still shouldn't be done when there are multiple threads running.
 *
 * lzma_init() initializes all internal static variables by calling
 * lzma_init_encoder() and lzma_init_decoder().
 *
 * If you need only encoder, decoder, or neither-encoder-nor-decoder
 * functions, you may use other initialization functions, which initialize
 * only a subset of liblzma's internal static variables. Using those
 * functions have the following advantages:
 *  - When linking statically against liblzma, less useless functions will
 *    get linked into the binary. E.g. if you need only the decoder functions,
 *    using lzma_init_decoder() avoids linking bunch of encoder related code.
 *  - There is less things to initialize, making the initialization
 *    process slightly faster.
 */
extern void lzma_init(void);


/**
 * \brief       Initialize internal static variables needed by encoders
 *
 * If you need only the encoder functions, you may use this function to
 * initialize only the things required by encoders.
 *
 * This function also calls lzma_init_check().
 */
extern void lzma_init_encoder(void);


/**
 * \brief       Initialize internal static variables needed by decoders
 *
 * If you need only the decoder functions, you may use this function to
 * initialize only the things required by decoders.
 *
 * This function also calls lzma_init_check().
 */
extern void lzma_init_decoder(void);


/**
 * \brief       Initialize internal static variables needed by integrity checks
 *
 * Currently this initializes CRC32 and CRC64 lookup tables if precalculated
 * tables haven't been built into the library. This function can be useful
 * if the only thing you need from liblzma is the integrity check functions.
 */
extern void lzma_init_check(void);
