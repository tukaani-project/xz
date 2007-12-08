/**
 * \file        lzma/block.h
 * \brief       .lzma Block handling
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
 * \brief       Options for the Block Header encoder and decoder
 *
 * Different things use different parts of this structure. Some read
 * some members, other functions write, and some do both. Only the
 * members listed for reading need to be initialized when the specified
 * functions are called. The members marked for writing will be assigned
 * new values at some point either by calling the given function or by
 * later calls to lzma_code().
 */
typedef struct {
	/**
	 * \brief       Type of integrity Check
	 *
	 * The type of the integrity Check is not stored into the Block
	 * Header, thus its value must be provided also when decoding.
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_check_type check;

	/**
	 * \brief       Precense of CRC32 of the Block Header
	 *
	 * Set this to true if CRC32 of the Block Header should be
	 * calculated and stored in the Block Header.
	 *
	 * There is no way to autodetect if CRC32 is present in the Block
	 * Header, thus this information must be provided also when decoding.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *  - lzma_block_header_decoder()
	 */
	lzma_bool has_crc32;

	/**
	 * \brief       Usage of End of Payload Marker
	 *
	 * If this is true, End of Payload Marker is used even if
	 * Uncompressed Size is known.
	 *
	 * Read by:
	 *  - lzma_block_header_encoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 */
	lzma_bool has_eopm;

	/**
	 * \brief       True if the Block is a Metadata Block
	 *
	 * If this is true, the Metadata bit will be set in the Block Header.
	 * It is up to the application to store correctly formatted data
	 * into Metadata Block.
	 *
	 * Read by:
	 *  - lzma_block_header_encoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 */
	lzma_bool is_metadata;

	/**
	 * \brief       True if Uncompressed Size is in Block Footer
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_bool has_uncompressed_size_in_footer;

	/**
	 * \brief       True if Backward Size is in Block Footer
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_bool has_backward_size;

	/**
	 * \brief       True if Block coder should take care of Padding
	 *
	 * In liblzma, Stream decoder sets this to true when decoding
	 * Header Metadata Block or Data Blocks from Multi-Block Stream,
	 * and to false when decoding Single-Block Stream or Footer
	 * Metadata Block from a Multi-Block Stream.
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_bool handle_padding;

	/**
	 * \brief       Size of the Compressed Data in bytes
	 *
	 * Usually you don't know this value when encoding in streamed mode.
	 * In non-streamed mode you can reserve space for this field when
	 * encoding the Block Header the first time, and then re-encode the
	 * Block Header and copy it over the original one after the encoding
	 * of the Block has been finished.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli compressed_size;

	/**
	 * \brief       Uncompressed Size in bytes
	 *
	 * Encoder: If this value is not LZMA_VLI_VALUE_UNKNOWN, it is stored
	 * to the Uncompressed Size field in the Block Header. The real
	 * uncompressed size of the data being compressed must match
	 * the Uncompressed Size or LZMA_HEADER_ERROR is returned.
	 *
	 * If Uncompressed Size is unknown, End of Payload Marker must
	 * be used. If uncompressed_size == LZMA_VLI_VALUE_UNKNOWN and
	 * has_eopm == 0, LZMA_HEADER_ERROR will be returned.
	 *
	 * Decoder: If this value is not LZMA_VLI_VALUE_UNKNOWN, it is
	 * compared to the real Uncompressed Size. If they do not match,
	 * LZMA_HEADER_ERROR is returned.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Number of bytes to reserve for Compressed Size
	 *
	 * This is useful if you want to be able to store the Compressed Size
	 * to the Block Header, but you don't know it when starting to encode.
	 * Setting this to non-zero value at maximum of LZMA_VLI_BYTES_MAX,
	 * the Block Header encoder will force the Compressed Size field to
	 * occupy specified number of bytes. You can later rewrite the Block
	 * Header to contain correct information by using otherwise identical
	 * lzma_options_block structure except the correct compressed_size.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 */
	uint32_t compressed_reserve;

	/**
	 * \brief       Number of bytes to reserve for Uncompressed Size
	 *
	 * See the description of compressed_size above.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder()
	 */
	uint32_t uncompressed_reserve;

	/**
	 * \brief       Total Size of the Block in bytes
	 *
	 * This is useful in the decoder, which can verify the Total Size
	 * if it is known from Index.
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli total_size;

	/**
	 * \brief       Upper limit of Total Size
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli total_limit;

	/**
	 * \brief       Upper limit of Uncompressed Size
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli uncompressed_limit;

	/**
	 * \brief       Array of filters
	 *
	 * There can be at maximum of seven filters. The end of the array
	 * is marked with .id = LZMA_VLI_VALUE_UNKNOWN. Minimum number of
	 * filters is zero; in that case, an implicit Copy filter is used.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decoder(): Note that this does NOT free()
	 *    the old filter options structures. If decoding fails, the
	 *    caller must take care of freeing the options structures
	 *    that may have been allocated and decoded before the error
	 *    occurred.
	 */
	lzma_options_filter filters[8];

	/**
	 * \brief       Size of the Padding field
	 *
	 * The Padding field exist to allow aligning the Compressed Data field
	 * optimally in the Block. See lzma_options_stream.alignment in
	 * stream.h for more information.
	 *
	 * If you want the Block Header encoder to automatically calculate
	 * optimal size for the Padding field by looking at the information
	 * in filters[], set this to LZMA_BLOCK_HEADER_PADDING_AUTO. In that
	 * case, you must also set the aligmnet variable to tell the the
	 * encoder the aligmnet of the beginning of the Block Header.
	 *
	 * The decoder never sets this to LZMA_BLOCK_HEADER_PADDING_AUTO.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder(): Note that this doesn't
	 *    accept LZMA_BLOCK_HEADER_PADDING_AUTO.
	 *
	 * Written by (these never set padding to
	 * LZMA_BLOCK_HEADER_PADDING_AUTO):
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_decoder()
	 */
	int32_t padding;
#	define LZMA_BLOCK_HEADER_PADDING_AUTO (-1)
#	define LZMA_BLOCK_HEADER_PADDING_MIN 0
#	define LZMA_BLOCK_HEADER_PADDING_MAX 31

	/**
	 * \brief       Alignment of the beginning of the Block Header
	 *
	 * This variable is read only if padding has been set to
	 * LZMA_BLOCK_HEADER_PADDING_AUTO.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encoder()
	 */
	uint32_t alignment;

	/**
	 * \brief       Size of the Block Header
	 *
	 * Read by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_decoder()
	 */
	uint32_t header_size;

} lzma_options_block;


/**
 * \brief       Calculates the size of Header Padding and Block Header
 *
 * \return      - LZMA_OK: Size calculated successfully and stored to
 *                options->header_size.
 *              - LZMA_HEADER_ERROR: Unsupported filters or filter options.
 *              - LZMA_PROG_ERROR: Invalid options
 *
 * \note        This doesn't check that all the options are valid i.e. this
 *              may return LZMA_OK even if lzma_block_header_encode() or
 *              lzma_block_encoder() would fail.
 */
extern lzma_ret lzma_block_header_size(lzma_options_block *options);


/**
 * \brief       Encodes Block Header
 *
 * Encoding of the Block options is done with a single call instead of
 * first initializing and then doing the actual work with lzma_code().
 *
 * \param       out         Beginning of the output buffer. This must be
 *                          at least options->header_size bytes.
 * \param       options     Block options to be encoded.
 *
 * \return      - LZMA_OK: Encoding was successful. options->header_size
 *                bytes were written to output buffer.
 *              - LZMA_HEADER_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_block_header_encode(
		uint8_t *out, const lzma_options_block *options);


/**
 * \brief       Initializes Block Header decoder
 *
 * Because the results of this decoder are placed into *options,
 * strm->next_in, strm->avail_in, and strm->total_in are not used.
 *
 * The only valid `action' with lzma_code() is LZMA_RUN.
 *
 * \return      - LZMA_OK: Encoding was successful. options->header_size
 *                bytes were written to output buffer.
 *              - LZMA_HEADER_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_block_header_decoder(
		lzma_stream *strm, lzma_options_block *options);


/**
 * \brief       Initializes .lzma Block encoder
 *
 * This function is required for multi-thread encoding. It may also be
 * useful when implementing custom file formats.
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_HEADER_ERROR
 *              - LZMA_DATA_ERROR: Limits (total_limit and uncompressed_limit)
 *                have been reached already.
 *              - LZMA_UNSUPPORTED_CHECK: options->check specfies a Check
 *                that is not supported by this buid of liblzma. Initializing
 *                the encoder failed.
 *              - LZMA_PROG_ERROR
 *
 * lzma_code() can return FIXME
 */
extern lzma_ret lzma_block_encoder(
		lzma_stream *strm, lzma_options_block *options);


/**
 * \brief       Initializes decoder for .lzma Block
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_UNSUPPORTED_CHECK: Initialization was successful, but
 *                the given Check type is not supported, thus Check will be
 *                ignored.
 *              - LZMA_PROG_ERROR
 *              - LZMA_MEM_ERROR
 */
extern lzma_ret lzma_block_decoder(
		lzma_stream *strm, lzma_options_block *options);
