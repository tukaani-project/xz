/**
 * \file        lzma/lzma.h
 * \brief       LZMA1 and LZMA2 filters
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
 * \brief       LZMA1 Filter ID
 *
 * LZMA1 is the very same thing as what was called just LZMA in earlier
 * LZMA Utils, 7-Zip, and LZMA SDK. It's called LZMA1 here to prevent
 * developers from accidentally using LZMA when they actually want LZMA2.
 */
#define LZMA_FILTER_LZMA1       LZMA_VLI_C(0x4000000000000001)

/**
 * \brief       LZMA2 Filter ID
 *
 * Usually you want this instead of LZMA1. Compared to LZMA1, LZMA2 adds
 * support for LZMA_SYNC_FLUSH, uncompressed chunks (expands uncompressible
 * data less), possibility to change lc/lp/pb in the middle of encoding, and
 * some other internal improvements.
 */
#define LZMA_FILTER_LZMA2       LZMA_VLI_C(0x21)


/**
 * \brief       Match finders
 *
 * Match finder has major effect on both speed and compression ratio.
 * Usually hash chains are faster than binary trees.
 *
 * The memory usage formulas are only rough estimates, which are closest to
 * reality when dict_size is a power of two. The formulas are  more complex
 * in reality, and can also change a little between liblzma versions. Use
 * lzma_memusage_encoder() to get more accurate estimate of memory usage.
 */
typedef enum {
	LZMA_MF_HC3     = 0x03,
		/**<
		 * \brief       Hash Chain with 2- and 3-byte hashing
		 *
		 * Minimum nice_len: 3
		 *
		 * Memory usage:
		 *  - dict_size <= 16 MiB: dict_size * 7.5
		 *  - dict_size > 16 MiB: dict_size * 5.5 + 64 MiB
		 */

	LZMA_MF_HC4     = 0x04,
		/**<
		 * \brief       Hash Chain with 2-, 3-, and 4-byte hashing
		 *
		 * Minimum nice_len: 4
		 *
		 * Memory usage: dict_size * 7.5
		 */

	LZMA_MF_BT2     = 0x12,
		/**<
		 * \brief       Binary Tree with 2-byte hashing
		 *
		 * Minimum nice_len: 2
		 *
		 * Memory usage: dict_size * 9.5
		 */

	LZMA_MF_BT3     = 0x13,
		/**<
		 * \brief       Binary Tree with 2- and 3-byte hashing
		 *
		 * Minimum nice_len: 3
		 *
		 * Memory usage:
		 *  - dict_size <= 16 MiB: dict_size * 11.5
		 *  - dict_size > 16 MiB: dict_size * 9.5 + 64 MiB
		 */

	LZMA_MF_BT4     = 0x14
		/**<
		 * \brief       Binary Tree with 2-, 3-, and 4-byte hashing
		 *
		 * Minimum nice_len: 4
		 *
		 * Memory usage: dict_size * 11.5
		 */
} lzma_match_finder;


/**
 * \brief       Test if given match finder is supported
 *
 * Returns true if the given match finder is supported by this liblzma build.
 * Otherwise false is returned. It is safe to call this with a value that
 * isn't listed in lzma_match_finder enumeration; the return value will be
 * false.
 *
 * There is no way to list which match finders are available in this
 * particular liblzma version and build. It would be useless, because
 * a new match finder, which the application developer wasn't aware,
 * could require giving additional options to the encoder that the older
 * match finders don't need.
 */
extern lzma_bool lzma_mf_is_supported(lzma_match_finder match_finder)
		lzma_attr_const;


/**
 * \brief       LZMA compression modes
 *
 * This selects the function used to analyze the data produced by the match
 * finder.
 */
typedef enum {
	LZMA_MODE_FAST = 1,
		/**<
		 * \brief       Fast compression
		 *
		 * Fast mode is usually at its best when combined with
		 * a hash chain match finder.
		 */

	LZMA_MODE_NORMAL = 2
		/**<
		 * \brief       Normal compression
		 *
		 * This is usually notably slower than fast mode. Use this
		 * together with binary tree match finders to expose the
		 * full potential of the LZMA encoder.
		 */
} lzma_mode;


/**
 * \brief       Test if given compression mode is supported
 *
 * Returns true if the given compression mode is supported by this liblzma
 * build. Otherwise false is returned. It is safe to call this with a value
 * that isn't listed in lzma_mode enumeration; the return value will be false.
 *
 * There is no way to list which modes are available in this particular
 * liblzma version and build. It would be useless, because a new compression
 * mode, which the application developer wasn't aware, could require giving
 * additional options to the encoder that the older modes don't need.
 */
extern lzma_bool lzma_mode_is_available(lzma_mode mode) lzma_attr_const;


/**
 * \brief       Options specific to the LZMA1 and LZMA2 filters
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
	 * Maximum size of the dictionary depends on multiple things:
	 *  - Memory usage limit
	 *  - Available address space (not a problem on 64-bit systems)
	 *  - Selected match finder (encoder only)
	 *
	 * Currently the maximum dictionary size for encoding is 1.5 GiB
	 * (i.e. (UINT32_C(1) << 30) + (UINT32_C(1) << 29)) even on 64-bit
	 * systems for certain match finder implementation reasons. In future,
	 * there may be match finders that support bigger dictionaries (3 GiB
	 * will probably be the maximum).
	 *
	 * Decoder already supports dictionaries up to 4 GiB - 1 B (i.e.
	 * UINT32_MAX), so increasing the maximum dictionary size of the
	 * encoder won't cause problems for old decoders.
	 *
	 * Because extremely small dictionaries sizes would have unneeded
	 * overhead in the decoder, the minimum dictionary size is 4096 bytes.
	 *
	 * \note        When decoding, too big dictionary does no other harm
	 *              than wasting memory.
	 */
	uint32_t dict_size;
#	define LZMA_DICT_SIZE_MIN       UINT32_C(4096)
#	define LZMA_DICT_SIZE_DEFAULT   (UINT32_C(1) << 23)

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
	const uint8_t *preset_dict;

	/**
	 * \brief       Size of the preset dictionary
	 *
	 * Specifies the size of the preset dictionary. If the size is
	 * bigger than dict_size, only the last dict_size bytes are processed.
	 *
	 * This variable is read only when preset_dict is not NULL.
	 */
	uint32_t preset_dict_size;

	/**
	 * \brief       Number of literal context bits
	 *
	 * How many of the highest bits of the previous uncompressed
	 * eight-bit byte (also known as `literal') are taken into
	 * account when predicting the bits of the next literal.
	 *
	 * \todo        Example
	 *
	 * There is a limit that applies to literal context bits and literal
	 * position bits together: lc + lp <= 4. Without this limit the
	 * decoding could become very slow, which could have security related
	 * results in some cases like email servers doing virus scanning.
	 * This limit also simplifies the internal implementation in liblzma.
	 *
	 * There may be LZMA streams that have lc + lp > 4 (maximum lc
	 * possible would be 8). It is not possible to decode such streams
	 * with liblzma.
	 */
	uint32_t lc;
#	define LZMA_LCLP_MIN    0
#	define LZMA_LCLP_MAX    4
#	define LZMA_LC_DEFAULT  3

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
	uint32_t lp;
#	define LZMA_LP_DEFAULT  0

	/**
	 * \brief       Number of position bits
	 *
	 * How many of the lowest bits of the current position in the
	 * uncompressed data is taken into account when estimating
	 * probabilities of matches. A match is a sequence of bytes for
	 * which a matching sequence is found from the dictionary and
	 * thus can be stored as distance-length pair.
	 *
	 * Example: If most of the matches occur at byte positions of
	 * 8 * n + 3, that is, 3, 11, 19, ... set pb to 3, because 2**3 == 8.
	 */
	uint32_t pb;
#	define LZMA_PB_MIN      0
#	define LZMA_PB_MAX      4
#	define LZMA_PB_DEFAULT  2

	/******************************************
	 * LZMA options needed only when encoding *
	 ******************************************/

	/**
	 * \brief       Indicate if the options structure is persistent
	 *
	 * If this is true, the application must keep this options structure
	 * available after the LZMA2 encoder has been initialized. With
	 * persistent structure it is possible to change some encoder options
	 * in the middle of the encoding process without resetting the encoder.
	 *
	 * This option is used only by LZMA2. LZMA1 ignores this and it is
	 * safe to not initialize this when encoding with LZMA1.
	 */
	lzma_bool persistent;

	/** LZMA compression mode */
	lzma_mode mode;

	/**
	 * \brief       Nice length of a match
	 *
	 * This determines how many bytes the encoder compares from the match
	 * candidates when looking for the best match. Once a match of at
	 * least nice_len bytes long is found, the encoder stops looking for
	 * better condidates and encodes the match. (Naturally, if the found
	 * match is actually longer than nice_len, the actual length is
	 * encoded; it's not truncated to nice_len.)
	 *
	 * Bigger values usually increase the compression ratio and
	 * compression time. For most files, 30 to 100 is a good value,
	 * which gives very good compression ratio at good speed.
	 *
	 * The exact minimum value depends on the match finder. The maximum is
	 * 273, which is the maximum length of a match that LZMA can encode.
	 */
	uint32_t nice_len;

	/** Match finder ID */
	lzma_match_finder mf;

	/**
	 * \brief       Maximum search depth in the match finder
	 *
	 * For every input byte, match finder searches through the hash chain
	 * or binary tree in a loop, each iteration going one step deeper in
	 * the chain or tree. The searching stops if
	 *  - a match of at least nice_len bytes long is found;
	 *  - all match candidates from the hash chain or binary tree have
	 *    been checked; or
	 *  - maximum search depth is reached.
	 *
	 * Maximum search depth is needed to prevent the match finder from
	 * wasting too much time in case there are lots of short match
	 * candidates. On the other hand, stopping the search before all
	 * candidates have been checked can reduce compression ratio.
	 *
	 * Setting depth to zero tells liblzma to use an automatic default
	 * value, that depends on the selected match finder and nice_len.
	 * The default is in the range [10, 200] or so (it may vary between
	 * liblzma versions).
	 *
	 * Using a bigger depth value than the default can increase
	 * compression ratio in some cases. There is no strict maximum value,
	 * but high values (thousands or millions) should be used with care:
	 * the encoder could remain fast enough with typical input, but
	 * malicious input could cause the match finder to slow down
	 * dramatically, possibly creating a denial of service attack.
	 */
	uint32_t depth;

	/*
	 * Reserved space to allow possible future extensions without
	 * breaking the ABI. You should not touch these, because the names
	 * of these variables may change. These are and will never be used
	 * with the currently supported options, so it is safe to leave these
	 * uninitialized.
	 */
	uint32_t reserved_int1;
	uint32_t reserved_int2;
	uint32_t reserved_int3;
	uint32_t reserved_int4;
	uint32_t reserved_int5;
	uint32_t reserved_int6;
	uint32_t reserved_int7;
	uint32_t reserved_int8;
	void *reserved_ptr1;
	void *reserved_ptr2;

} lzma_options_lzma;


/**
 * \brief       Set a compression level preset to lzma_options_lzma structure
 *
 * level = 1 is the fastest and level = 9 is the slowest. These presets match
 * the switches -1 .. -9 of the command line tool.
 *
 * The preset values are subject to changes between liblzma versions.
 *
 * This function is available only if LZMA encoder has been enabled.
 */
extern lzma_bool lzma_lzma_preset(lzma_options_lzma *options, uint32_t level);
