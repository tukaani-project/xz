/**
 * \file        lzma/index.h
 * \brief       Handling of Index lists
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
 * \brief       Opaque data type to hold the Index
 */
typedef struct lzma_index_s lzma_index;


/**
 * \brief       Index Record and its location
 */
typedef struct {
	/**
	 * Total Size of a Block.
	 */
	lzma_vli total_size;

	/**
	 * Uncompressed Size of a Block
	 */
	lzma_vli uncompressed_size;

	/**
	 * Offset of the first byte of a Block relative to the beginning
	 * of the Stream, or if there are multiple Indexes combined,
	 * relative to the beginning of the first Stream.
	 */
	lzma_vli stream_offset;

	/**
	 * Uncompressed offset
	 */
	lzma_vli uncompressed_offset;

} lzma_index_record;


/**
 * \brief       Allocate and initialize a new lzma_index structure
 *
 * If i is NULL, a new lzma_index structure is allocated, initialized,
 * and a pointer to it returned. If allocation fails, NULL is returned.
 *
 * If i is non-NULL, it is reinitialized and the same pointer returned.
 * In this case, return value cannot be NULL or a different pointer than
 * the i given as argument.
 */
extern lzma_index *lzma_index_init(lzma_index *i, lzma_allocator *allocator)
		lzma_attr_warn_unused_result;


/**
 * \brief       Deallocate the Index
 */
extern void lzma_index_end(lzma_index *i, lzma_allocator *allocator);


/**
 * \brief       Add a new Record to an Index
 *
 * \param       index             Pointer to a lzma_index structure
 * \param       total_size        Total Size of a Block
 * \param       uncompressed_size Uncompressed Size of a Block, or
 *                                LZMA_VLI_UNKNOWN to indicate padding.
 *
 * Appending a new Record does not affect the read position.
 *
 * \return      - LZMA_OK
 *              - LZMA_DATA_ERROR: Compressed or uncompressed size of the
 *                Stream or size of the Index field would grow too big.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_index_append(lzma_index *i, lzma_allocator *allocator,
		lzma_vli total_size, lzma_vli uncompressed_size)
		lzma_attr_warn_unused_result;


/**
 * \brief       Get the number of Records
 */
extern lzma_vli lzma_index_count(const lzma_index *i) lzma_attr_pure;


/**
 * \brief       Get the size of the Index field as bytes
 *
 * This is needed to verify the Index Size field from the Stream Footer.
 */
extern lzma_vli lzma_index_size(const lzma_index *i) lzma_attr_pure;


/**
 * \brief       Get the total size of the Blocks
 *
 * This doesn't include the Stream Header, Stream Footer, Stream Padding,
 * or Index fields.
 */
extern lzma_vli lzma_index_total_size(const lzma_index *i) lzma_attr_pure;


/**
 * \brief       Get the total size of the Stream
 *
 * If multiple Indexes have been combined, this works as if the Blocks
 * were in a single Stream.
 */
extern lzma_vli lzma_index_stream_size(const lzma_index *i) lzma_attr_pure;


/**
 * \brief       Get the total size of the file
 *
 * When no Indexes have been combined with lzma_index_cat(), this function is
 * identical to lzma_index_stream_size(). If multiple Indexes have been
 * combined, this includes also the possible Stream Padding fields.
 */
extern lzma_vli lzma_index_file_size(const lzma_index *i) lzma_attr_pure;


/**
 * \brief       Get the uncompressed size of the Stream
 */
extern lzma_vli lzma_index_uncompressed_size(const lzma_index *i)
		lzma_attr_pure;


/**
 * \brief       Get the next Record from the Index
 */
extern lzma_bool lzma_index_read(lzma_index *i, lzma_index_record *record)
		lzma_attr_warn_unused_result;


/**
 * \brief       Rewind the Index
 *
 * Rewind the Index so that next call to lzma_index_read() will return the
 * first Record.
 */
extern void lzma_index_rewind(lzma_index *i);


/**
 * \brief       Locate a Record
 *
 * When the Index is available, it is possible to do random-access reading
 * with granularity of Block size.
 *
 * \param       i       Pointer to lzma_index structure
 * \param       record  Pointer to a structure to hold the search results
 * \param       target  Uncompressed target offset
 *
 * If the target is smaller than the uncompressed size of the Stream (can be
 * checked with lzma_index_uncompressed_size()):
 *  - Information about the Record containing the requested uncompressed
 *    offset is stored into *record.
 *  - Read offset will be adjusted so that calling lzma_index_read() can be
 *    used to read subsequent Records.
 *  - This function returns false.
 *
 * If target is greater than the uncompressed size of the Stream, *record
 * and the read position are not modified, and this function returns true.
 */
extern lzma_bool lzma_index_locate(
		lzma_index *i, lzma_index_record *record, lzma_vli target)
		lzma_attr_warn_unused_result;


/**
 * \brief       Concatenate Indexes of two Streams
 *
 *
 *
 * \param       dest      Destination Index after which src is appended Source
 * \param       src       Index. The memory allocated for this is either moved
 *                        to be part of *dest or freed iff the function call
 *                        succeeds, and src will be an invalid pointer.
 * \param       allocator Custom memory allocator; can be NULL to use
 *                        malloc() and free().
 * \param       padding   Size of the Stream Padding field between Streams.
 *
 * \return      - LZMA_OK: Indexes concatenated successfully.
 *              - LZMA_DATA_ERROR: *dest would grow too big.
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_index_cat(lzma_index *lzma_restrict dest,
		lzma_index *lzma_restrict src,
		lzma_allocator *allocator, lzma_vli padding)
		lzma_attr_warn_unused_result;


/**
 * \brief       Duplicates an Index list
 *
 * \return      A copy of the Index, or NULL if memory allocation failed.
 */
extern lzma_index *lzma_index_dup(
		const lzma_index *i, lzma_allocator *allocator)
		lzma_attr_warn_unused_result;


/**
 * \brief       Compares if two Index lists are identical
 */
extern lzma_bool lzma_index_equal(const lzma_index *a, const lzma_index *b)
		lzma_attr_pure;


/**
 * \brief       Initializes Index encoder
 */
extern lzma_ret lzma_index_encoder(lzma_stream *strm, lzma_index *i)
		lzma_attr_warn_unused_result;


/**
 * \brief       Initializes Index decoder
 */
extern lzma_ret lzma_index_decoder(lzma_stream *strm, lzma_index **i)
		lzma_attr_warn_unused_result;
