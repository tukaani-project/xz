///////////////////////////////////////////////////////////////////////////////
//
/// \file       index.c
/// \brief      Handling of Index in Metadata
//
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


/**
 * \brief       Duplicates an Index list
 *
 * \return      A copy of the Index list, or NULL if memory allocation
 *              failed or the original Index was empty.
 */
extern LZMA_API lzma_index *
lzma_index_dup(const lzma_index *old_current, lzma_allocator *allocator)
{
	lzma_index *new_head = NULL;
	lzma_index *new_current = NULL;

	while (old_current != NULL) {
		lzma_index *i = lzma_alloc(sizeof(lzma_index), allocator);
		if (i == NULL) {
			lzma_index_free(new_head, allocator);
			return NULL;
		}

		i->total_size = old_current->total_size;
		i->uncompressed_size = old_current->uncompressed_size;
		i->next = NULL;

		if (new_head == NULL)
			new_head = i;
		else
			new_current->next = i;

		new_current = i;
		old_current = old_current->next;
	}

	return new_head;
}


/**
 * \brief       Frees an Index list
 *
 * All Index Recors in the list are freed. This function is convenient when
 * getting rid of lzma_metadata structures containing an Index.
 */
extern LZMA_API void
lzma_index_free(lzma_index *i, lzma_allocator *allocator)
{
	while (i != NULL) {
		lzma_index *tmp = i->next;
		lzma_free(i, allocator);
		i = tmp;
	}

	return;
}


/**
 * \brief       Calculates properties of an Index list
 *
 *
 */
extern LZMA_API lzma_ret
lzma_index_count(const lzma_index *i, size_t *count,
		lzma_vli *lzma_restrict total_size,
		lzma_vli *lzma_restrict uncompressed_size)
{
	*count = 0;
	*total_size = 0;
	*uncompressed_size = 0;

	while (i != NULL) {
		if (i->total_size == LZMA_VLI_VALUE_UNKNOWN) {
			*total_size = LZMA_VLI_VALUE_UNKNOWN;
		} else if (i->total_size > LZMA_VLI_VALUE_MAX) {
			return LZMA_PROG_ERROR;
		} else if (*total_size != LZMA_VLI_VALUE_UNKNOWN) {
			*total_size += i->total_size;
			if (*total_size > LZMA_VLI_VALUE_MAX)
				return LZMA_PROG_ERROR;
		}

		if (i->uncompressed_size == LZMA_VLI_VALUE_UNKNOWN) {
			*uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
		} else if (i->uncompressed_size > LZMA_VLI_VALUE_MAX) {
			return LZMA_PROG_ERROR;
		} else if (*uncompressed_size != LZMA_VLI_VALUE_UNKNOWN) {
			*uncompressed_size += i->uncompressed_size;
			if (*uncompressed_size > LZMA_VLI_VALUE_MAX)
				return LZMA_PROG_ERROR;
		}

		++*count;
		i = i->next;
	}

	// FIXME ?
	if (*total_size == LZMA_VLI_VALUE_UNKNOWN
			|| *uncompressed_size == LZMA_VLI_VALUE_UNKNOWN)
		return LZMA_HEADER_ERROR;

	return LZMA_OK;
}



extern LZMA_API lzma_bool
lzma_index_is_equal(const lzma_index *a, const lzma_index *b)
{
	while (a != NULL && b != NULL) {
		if (a->total_size != b->total_size || a->uncompressed_size
				!= b->uncompressed_size)
			return false;

		a = a->next;
		b = b->next;
	}

	return a == b;
}
