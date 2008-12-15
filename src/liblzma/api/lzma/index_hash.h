/**
 * \file        lzma/index_hash.h
 * \brief       Validates Index by using a hash function
 *
 * Instead of constructing complete Index while decoding Blocks, Index hash
 * calculates a hash of the Block sizes and Index, and then compares the
 * hashes. This way memory usage is constant even with large number of
 * Blocks and huge Index.
 *
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
 * \brief       Opaque data type to hold the Index hash
 */
typedef struct lzma_index_hash_s lzma_index_hash;


/**
 * \brief       Allocate and initialize a new lzma_index_hash structure
 *
 * If index_hash is NULL, a new lzma_index_hash structure is allocated,
 * initialized, and a pointer to it returned. If allocation fails, NULL
 * is returned.
 *
 * If index_hash is non-NULL, it is reinitialized and the same pointer
 * returned. In this case, return value cannot be NULL or a different
 * pointer than the index_hash that was given as an argument.
 */
extern lzma_index_hash *lzma_index_hash_init(
		lzma_index_hash *index_hash, lzma_allocator *allocator)
		lzma_attr_warn_unused_result;


/**
 * \brief       Deallocate lzma_index_hash structure
 */
extern void lzma_index_hash_end(
		lzma_index_hash *index_hash, lzma_allocator *allocator);


/**
 * \brief       Add a new Record to an Index hash
 *
 * \param       index             Pointer to a lzma_index_hash structure
 * \param       unpadded_size     Unpadded Size of a Block
 * \param       uncompressed_size Uncompressed Size of a Block
 *
 * \return      - LZMA_OK
 *              - LZMA_DATA_ERROR: Compressed or uncompressed size of the
 *                Stream or size of the Index field would grow too big.
 *              - LZMA_PROG_ERROR: Invalid arguments or this function is being
 *                used when lzma_index_hash_decode() has already been used.
 */
extern lzma_ret lzma_index_hash_append(lzma_index_hash *index_hash,
		lzma_vli unpadded_size, lzma_vli uncompressed_size)
		lzma_attr_warn_unused_result;


/**
 * \brief       Decode and validate the Index field
 *
 * After telling the sizes of all Blocks with lzma_index_hash_append(),
 * the actual Index field is decoded with this function. Specifically,
 * once decoding of the Index field has been started, no more Records
 * can be added using lzma_index_hash_append().
 *
 * This function doesn't use lzma_stream structure to pass the input data.
 * Instead, the input buffer is specified using three arguments. This is
 * because it matches better the internal APIs of liblzma.
 *
 * \param       index_hash      Pointer to a lzma_index_hash structure
 * \param       in              Pointer to the beginning of the input buffer
 * \param       in_pos          in[*in_pos] is the next byte to process
 * \param       in_size         in[in_size] is the first byte not to process
 *
 * \return      - LZMA_OK: So far good, but more input is needed.
 *              - LZMA_STREAM_END: Index decoded successfully and it matches
 *                the Records given with lzma_index_hash_append().
 *              - LZMA_DATA_ERROR: Index is corrupt or doesn't match the
 *                information given with lzma_index_hash_append().
 *              - LZMA_BUF_ERROR: Cannot progress because *in_pos >= in_size.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_index_hash_decode(lzma_index_hash *index_hash,
		const uint8_t *in, size_t *in_pos, size_t in_size)
		lzma_attr_warn_unused_result;


/**
 * \brief       Get the size of the Index field as bytes
 *
 * This is needed to verify the Backward Size field in the Stream Footer.
 */
extern lzma_vli lzma_index_hash_size(const lzma_index_hash *index_hash)
		lzma_attr_pure;
