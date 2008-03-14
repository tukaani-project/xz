/**
 * \file        lzma/lzma.h
 * \brief       LZMA filter
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
 * \brief       Filter ID
 *
 * Filter ID of the LZMA filter. This is used as lzma_options_filter.id.
 */
#define LZMA_FILTER_LZMA        LZMA_VLI_C(0x40)


/**
 * \brief       LZMA compression modes
 *
 * Currently there are only two modes. Earlier LZMA SDKs had also third
 * mode between fast and best.
 */
typedef enum {
	LZMA_MODE_INVALID = -1,
		/**<
		 * \brief       Invalid mode
		 *
		 * Used as array terminator in lzma_available_modes.
		 */


	LZMA_MODE_FAST = 0,
		/**<
		 * \brief       Fast compression
		 *
		 * Fast mode is usually at its best when combined with
		 * a hash chain match finder.
		 */

	LZMA_MODE_BEST = 2
		/**<
		 * \brief       Best compression ratio
		 *
		 * This is usually notably slower than fast mode. Use this
		 * together with binary tree match finders to expose the
		 * full potential of the LZMA encoder.
		 */
} lzma_mode;


/**
 * \brief       Match finders
 *
 * Match finder has major effect on both speed and compression ratio.
 * Usually hash chains are faster than binary trees.
 */
typedef enum {
	LZMA_MF_INVALID = -1,
		/**<
		 * \brief       Invalid match finder ID
		 *
		 * Used as array terminator in lzma_available_match_finders.
		 */

	LZMA_MF_HC3     = 0x03,
		/**<
		 * \brief       Hash Chain with 3 bytes hashing
		 *
		 * \todo Memory requirements
		 *
		 * \note        It's possible that this match finder gets
		 *              removed in future. The definition will stay
		 *              in this header, but liblzma may return
		 *              LZMA_HEADER_ERROR if it is specified (just
		 *              like it would if the match finder had been
		 *              disabled at compile time).
		 */

	LZMA_MF_HC4     = 0x04,
		/**<
		 * \brief       Hash Chain with 4 bytes hashing
		 *
		 * Memory requirements: 7.5 * dictionary_size + 4 MiB
		 *
		 * \note        It's possible that this match finder gets
		 *              removed in future. The definition will stay
		 *              in this header, but liblzma may return
		 *              LZMA_HEADER_ERROR if it is specified (just
		 *              like it would if the match finder had been
		 *              disabled at compile time).
		 */

	LZMA_MF_BT2     = 0x12,
		/**<
		 * \brief       Binary Tree with 2 bytes hashing
		 *
		 * Memory requirements: 9.5 * dictionary_size + 4 MiB
		 */

	LZMA_MF_BT3     = 0x13,
		/**<
		 * \brief       Binary Tree with 3 bytes hashing
		 *
		 * Memory requirements: 11.5 * dictionary_size + 4 MiB
		 */

	LZMA_MF_BT4     = 0x14
		/**<
		 * \brief       Binary Tree with 4 bytes hashing
		 *
		 * Memory requirements: 11.5 * dictionary_size + 4 MiB
		 */
} lzma_match_finder;


/**
 * \brief       Options specific to the LZMA method handler
 */
typedef struct {
	/**********************************
	 * LZMA encoding/decoding options *
	 **********************************/

	/* These options are required in encoder and also with raw decoding. */

	/**
	 * \brief       Dictionary size in bytes
	 *
	 * Dictionary size indicates how many bytes of the recently processed
	 * uncompressed data is kept in memory. One method to reduce size of
	 * the uncompressed data is to store distance-length pairs, which
	 * indicate what data to repeat from the dictionary buffer. Thus,
	 * the bigger the dictionary, the better compression ratio usually is.
	 *
	 * Raw decoding: Too big dictionary does no other harm than
	 * wasting memory. This value is ignored by lzma_raw_decode_buffer(),
	 * because it uses the target buffer as the dictionary.
	 */
	uint32_t dictionary_size;
#	define LZMA_DICTIONARY_SIZE_MIN            1
#	define LZMA_DICTIONARY_SIZE_MAX            (UINT32_C(1) << 30)
#	define LZMA_DICTIONARY_SIZE_DEFAULT        (UINT32_C(1) << 23)

	/**
	 * \brief       Number of literal context bits
	 *
	 * How many of the highest bits of the previous uncompressed
	 * eight-bit byte (also known as `literal') are taken into
	 * account when predicting the bits of the next literal.
	 *
	 * \todo        Example
	 */
	uint32_t literal_context_bits;
#	define LZMA_LITERAL_CONTEXT_BITS_MIN       0
#	define LZMA_LITERAL_CONTEXT_BITS_MAX       8
#	define LZMA_LITERAL_CONTEXT_BITS_DEFAULT   3

	/**
	 * \brief       Number of literal position bits
	 *
	 * How many of the lowest bits of the current position (number
	 * of bytes from the beginning of the uncompressed data) in the
	 * uncompressed data is taken into account when predicting the
	 * bits of the next literal (a single eight-bit byte).
	 *
	 * \todo        Example
	 */
	uint32_t literal_pos_bits;
#	define LZMA_LITERAL_POS_BITS_MIN           0
#	define LZMA_LITERAL_POS_BITS_MAX           4
#	define LZMA_LITERAL_POS_BITS_DEFAULT       0

	/**
	 * \brief       Number of position bits
	 *
	 * How many of the lowest bits of the current position in the
	 * uncompressed data is taken into account when estimating
	 * probabilities of matches. A match is a sequence of bytes for
	 * which a matching sequence is found from the dictionary and
	 * thus can be stored as distance-length pair.
	 *
	 * Example: If most of the matches occur at byte positions
	 * of 8 * n + 3, that is, 3, 11, 19, ... set pos_bits to 3,
	 * because 2**3 == 8.
	 */
	uint32_t pos_bits;
#	define LZMA_POS_BITS_MIN                   0
#	define LZMA_POS_BITS_MAX                   4
#	define LZMA_POS_BITS_DEFAULT               2

	/**
	 * \brief       Pointer to an initial dictionary
	 *
	 * It is possible to initialize the LZ77 history window using
	 * a preset dictionary. Here is a good quote from zlib's
	 * documentation; this applies to LZMA as is:
	 *
	 * "The dictionary should consist of strings (byte sequences) that
	 * are likely to be encountered later in the data to be compressed,
	 * with the most commonly used strings preferably put towards the
	 * end of the dictionary. Using a dictionary is most useful when
	 * the data to be compressed is short and can be predicted with
	 * good accuracy; the data can then be compressed better than
	 * with the default empty dictionary."
	 * (From deflateSetDictionary() in zlib.h of zlib version 1.2.3)
	 *
	 * This feature should be used only in special situations.
	 * It works correctly only with raw encoding and decoding.
	 * Currently none of the container formats supported by
	 * liblzma allow preset dictionary when decoding, thus if
	 * you create a .lzma file with preset dictionary, it cannot
	 * be decoded with the regular .lzma decoder functions.
	 *
	 * \todo        This feature is not implemented yet.
	 */
	const uint8_t *preset_dictionary;

	/**
	 * \brief       Size of the preset dictionary
	 *
	 * Specifies the size of the preset dictionary. If the size is
	 * bigger than dictionary_size, only the last dictionary_size
	 * bytes are processed.
	 *
	 * This variable is read only when preset_dictionary is not NULL.
	 */
	uint32_t preset_dictionary_size;

	/******************************************
	 * LZMA options needed only when encoding *
	 ******************************************/

	/** LZMA compression mode */
	lzma_mode mode;

	/**
	 * \brief       Number of fast bytes
	 *
	 * Number of fast bytes determines how many bytes the encoder
	 * compares from the match candidates when looking for the best
	 * match. Bigger fast bytes value usually increase both compression
	 * ratio and time.
	 */
	uint32_t fast_bytes;
#	define LZMA_FAST_BYTES_MIN                 5
#	define LZMA_FAST_BYTES_MAX                 273
#	define LZMA_FAST_BYTES_DEFAULT             128

	/** Match finder ID */
	lzma_match_finder match_finder;

	/**
	 * \brief       Match finder cycles
	 *
	 * Higher values give slightly better compression ratio but
	 * decrease speed. Use special value 0 to let liblzma use
	 * match-finder-dependent default value.
	 *
	 * \todo        Write much better description.
	 */
	uint32_t match_finder_cycles;

} lzma_options_lzma;


/**
 * \brief       Available LZMA encoding modes
 *
 * Pointer to an array containing the list of available encoding modes.
 *
 * This variable is available only if LZMA encoder has been enabled.
 */
extern const lzma_mode *const lzma_available_modes;


/**
 * \brief       Available match finders
 *
 * Pointer to an array containing the list of available match finders.
 * The last element is LZMA_MF_INVALID.
 *
 * This variable is available only if LZMA encoder has been enabled.
 */
extern const lzma_match_finder *const lzma_available_match_finders;


/**
 * \brief       Table of presets for the LZMA filter
 *
 * lzma_preset_lzma[0] is the fastest and lzma_preset_lzma[8] is the slowest.
 * These presets match the switches -1 .. -9 of the lzma command line tool
 *
 * The preset values are subject to changes between liblzma versions.
 *
 * This variable is available only if LZMA encoder has been enabled.
 */
extern const lzma_options_lzma lzma_preset_lzma[9];
