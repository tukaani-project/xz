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
 * \brief
 *
 * FIXME desc
 */
typedef struct lzma_index_s lzma_index;
struct lzma_index_s {
	/**
	 * \brief       Total Size of the Block
	 *
	 * This includes Block Header, Compressed Data, and Block Footer.
	 */
	lzma_vli total_size;

	/**
	 * \brief       Uncompressed Size of the Block
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Pointer to the next Index Record
	 *
	 * This is NULL on the last Index Record.
	 */
	lzma_index *next;
};


/**
 * \brief       Duplicates an Index list
 *
 * \return      A copy of the Index list, or NULL if memory allocation
 *              failed or the original Index was empty.
 */
extern lzma_index *lzma_index_dup(
		const lzma_index *index, lzma_allocator *allocator);


/**
 * \brief       Frees an Index list
 *
 * All Index Recors in the list are freed. This function is convenient when
 * getting rid of lzma_metadata structures containing an Index.
 */
extern void lzma_index_free(lzma_index *index, lzma_allocator *allocator);


/**
 * \brief       Calculates information about the Index
 *
 * \return      LZMA_OK on success, LZMA_PROG_ERROR on error. FIXME
 */
extern lzma_ret lzma_index_count(const lzma_index *index, size_t *count,
		lzma_vli *lzma_restrict total_size,
		lzma_vli *lzma_restrict uncompressed_size);


/**
 * \brief       Compares if two Index lists are identical
 */
extern lzma_bool lzma_index_is_equal(const lzma_index *a, const lzma_index *b);
