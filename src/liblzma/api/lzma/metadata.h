/**
 * \file        lzma/metadata.h
 * \brief       Metadata handling
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
 * \brief       Information stored into a Metadata Block
 *
 * This structure holds all the information that can be stored to
 * a Metadata Block.
 */
typedef struct {
	/**
	 * \brief       Size of Header Metadata Block
	 */
	lzma_vli header_metadata_size;

	/**
	 * \brief       Total Size of the Stream
	 */
	lzma_vli total_size;

	/**
	 * \brief       Uncompressed Size of the Stream
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Index of the Blocks stored in the Stream
	 */
	lzma_index *index;

	/**
	 * \brief       Extra information
	 */
	lzma_extra *extra;

} lzma_metadata;


/**
 * \brief       Calculate the encoded size of Metadata
 *
 * \return      Uncompressed size of the Metadata in encoded form. This value
 *              may be passed to Block encoder as Uncompressed Size when using
 *              Metadata filter. On error, zero is returned.
 */
extern lzma_vli lzma_metadata_size(const lzma_metadata *metadata);


/**
 * \brief       Initializes Metadata encoder
 *
 * \param       coder       Pointer to a pointer to hold Metadata encoder's
 *                          internal state. Original value is ignored, thus
 *                          you don't need to initialize the pointer.
 * \param       allocator   Custom memory allocator; usually NULL.
 * \param       metadata    Pointer to Metadata to encoded
 *
 * \return      - LZMA_OK: Initialization succeeded.
 *              - LZMA_MEM_ERROR: Cannot allocate memory for *coder.
 *
 * The initialization function makes internal copy of the *metadata structure.
 * However, the linked lists metadata->index and metadata->extra are NOT
 * copied. Thus, the application may destroy *metadata after initialization
 * if it likes, but not Index or Extra.
 */
extern lzma_ret lzma_metadata_encoder(lzma_stream *strm,
		lzma_options_block *options, const lzma_metadata *metadata);


/**
 * \brief       Initializes Metadata decoder
 *
 * \param       want_extra    If this is true, Extra Records will be stored
 *                            to metadata->extra. If this is false, Extra
 *                            Records will be parsed but not stored anywhere,
 *                            metadata->extra will be set to NULL.
 */
extern lzma_ret lzma_metadata_decoder(
		lzma_stream *strm, lzma_options_block *options,
		lzma_metadata *metadata, lzma_bool want_extra);
