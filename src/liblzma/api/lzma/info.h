/**
 * \file        lzma/info.h
 * \brief       Handling of Stream size information
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


/**********
 * Basics *
 **********/

/**
 * \brief       Opaque data type to hold the size information
 */
typedef struct lzma_info_s lzma_info;


typedef struct {
	/**
	 * \brief       Total Size of this Block
	 *
	 * This can be LZMA_VLI_VALUE_UNKNOWN.
	 */
	lzma_vli total_size;

	/**
	 * \brief       Uncompressed Size of this Block
	 *
	 * This can be LZMA_VLI_VALUE_UNKNOWN.
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Offset of the first byte of the Block
	 *
	 * In encoder, this is useful to find out the alignment of the Block.
	 *
	 * In decoder, this is useful when doing random-access reading
	 * with help from lzma_info_data_locate().
	 */
	lzma_vli stream_offset;

	/**
	 * \brief       Uncompressed offset of the Block
	 *
	 * Offset of the first uncompressed byte of the Block relative to
	 * all uncompressed data in the Block.
	 * FIXME desc
	 */
	lzma_vli uncompressed_offset;

	/**
	 * \brief       Pointers to internal data structures
	 *
	 * Applications must not touch these.
	 */
	void *internal[4];

} lzma_info_iter;


typedef enum {
	LZMA_INFO_STREAM_START,
	LZMA_INFO_HEADER_METADATA,
	LZMA_INFO_TOTAL,
	LZMA_INFO_UNCOMPRESSED,
	LZMA_INFO_FOOTER_METADATA
} lzma_info_size;


/**
 * \brief       Allocates and initializes a new lzma_info structure
 *
 * If info is NULL, a new lzma_info structure is allocated, initialized, and
 * a pointer to it returned. If allocation fails, NULL is returned.
 *
 * If info is non-NULL, it is reinitialized and the same pointer returned.
 * (In this case, return value cannot be NULL or a different pointer than
 * the info given as argument.)
 */
extern lzma_info *lzma_info_init(lzma_info *info, lzma_allocator *allocator);


/**
 * \brief       Resets lzma_info
 *
 * This is like calling lzma_info_end() and lzma_info_create(), but
 * re-uses the existing base structure.
 */
extern void lzma_info_reset(
		lzma_info *info, lzma_allocator *allocator);


/**
 * \brief       Frees memory allocated for a lzma_info structure
 */
extern void lzma_info_free(lzma_info *info, lzma_allocator *allocator);


/************************
 * Setting known values *
 ************************/

/**
 * \brief       Set a known size value
 *
 * \param       info    Pointer returned by lzma_info_create()
 * \param       type    Any value from lzma_info_size
 * \param       size    Value to set or verify
 *
 * \return      LZMA_OK on success, LZMA_DATA_ERROR if the size doesn't
 *              match the existing information, or LZMA_PROG_ERROR
 *              if type is invalid or size is not a valid VLI.
 */
extern lzma_ret lzma_info_size_set(
		lzma_info *info, lzma_info_size type, lzma_vli size);


/**
 * \brief       Sets the Index
 *
 * The given lzma_index list is "absorbed" by this function. The application
 * must not access it after this function call, even if this function returns
 * an error.
 *
 * \note        The given lzma_index will at some point get freed by the
 *              lzma_info_* functions. If you use a custom lzma_allocator,
 *              make sure that it can free the lzma_index.
 */
extern lzma_ret lzma_info_index_set(
		lzma_info *info, lzma_allocator *allocator,
		lzma_index *index, lzma_bool eat_index);


/**
 * \brief       Sets information from a known Metadata Block
 *
 * This is a shortcut for calling lzma_info_size_set() with different type
 * arguments, lzma_info_index_set() with metadata->index.
 */
extern lzma_ret lzma_info_metadata_set(lzma_info *info,
		lzma_allocator *allocator, lzma_metadata *metadata,
		lzma_bool is_header_metadata, lzma_bool eat_index);


/***************
 * Incremental *
 ***************/

/**
 * \brief       Prepares an iterator to be used with given lzma_info structure
 *
 *
 */
extern void lzma_info_iter_begin(lzma_info *info, lzma_info_iter *iter);


/**
 * \brief       Moves to the next Index Record
 *
 *
 */
extern lzma_ret lzma_info_iter_next(
		lzma_info_iter *iter, lzma_allocator *allocator);


/**
 * \brief       Sets or verifies the sizes in the Index Record
 *
 * \param       iter    Pointer to iterator to be set or verified
 * \param       total_size
 *                      Total Size in bytes or LZMA_VLI_VALUE_UNKNOWN
 * \param       uncompressed_size
 *                      Uncompressed Size or LZMA_VLI_VALUE_UNKNOWN
 *
 * \return      - LZMA_OK: All OK.
 *              - LZMA_DATA_ERROR: Given sizes don't match with the already
 *                known sizes.
 *              - LZMA_PROG_ERROR: Internal error, possibly integer
 *                overflow (e.g. the sum of all the known sizes is too big)
 */
extern lzma_ret lzma_info_iter_set(lzma_info_iter *iter,
		lzma_vli total_size, lzma_vli uncompressed_size);


/**
 * \brief       Locates a Data Block
 *
 * \param       iter        Properly initialized iterator
 * \param       allocator   Pointer to lzma_allocator or NULL
 * \param       uncompressed_offset
 *                          Target offset to locate. The final offset
 *                          will be equal or smaller than this.
 * \param       allow_alloc True if this function is allowed to call
 *                          lzma_info_iter_next() to allocate a new Record
 *                          if the requested offset reached end of Index
 *                          Record list. Note that if Index has been marked
 *                          final, lzma_info_iter_next() is never called.
 *
 * \return      - LZMA_OK: All OK, *iter updated accordingly.
 *              - LZMA_DATA_ERROR: Trying to search past the end of the Index
 *                Record list, and allocating a new Record was not allowed
 *                either because allow_alloc was false or Index was final.
 *              - LZMA_PROG_ERROR: Internal error (probably integer
 *                overflow causing some lzma_vli getting too big).
 */
extern lzma_ret lzma_info_iter_locate(lzma_info_iter *iter,
		lzma_allocator *allocator, lzma_vli uncompressed_offset,
		lzma_bool allow_alloc);


/**
 * \brief       Finishes incrementally constructed Index
 *
 * This sets the known Total Size and Uncompressed of the Data Blocks
 * based on the information collected from the Index Records, and marks
 * the Index as final.
 */
extern lzma_ret lzma_info_index_finish(lzma_info *info);


/***************************
 * Reading the information *
 ***************************/

/**
 * \brief       Gets a known size
 *
 *
 */
extern lzma_vli lzma_info_size_get(
		const lzma_info *info, lzma_info_size type);

extern lzma_vli lzma_info_metadata_locate(
		const lzma_info *info, lzma_bool is_header_metadata);

/**
 * \brief       Gets a pointer to the beginning of the Index list
 *
 * If detach is true, the Index will be detached from the lzma_info
 * structure, and thus not be modified or freed by lzma_info_end().
 *
 * If detach is false, the application must not modify the Index in any way.
 * Also, the Index list is guaranteed to be valid only till the next call
 * to any lzma_info_* function.
 */
extern lzma_index *lzma_info_index_get(lzma_info *info, lzma_bool detach);


extern size_t lzma_info_index_count_get(const lzma_info *info);


extern uint32_t lzma_info_metadata_alignment_get(
		const lzma_info *info, lzma_bool is_header_metadata);



/**
 * \brief       Locate a Block containing the given uncompressed offset
 *
 * This function is useful when you need to do random-access reading in
 * a Multi-Block Stream.
 *
 * \param       info    Pointer to lzma_info that has at least one
 *                      Index Record. The Index doesn't need to be finished.
 * \param       uncompressed_target
 *                      Uncompressed target offset which the caller would
 *                      like to locate from the Stream.
 * \param       stream_offset
 *                      Starting offset (relative to the beginning the Stream)
 *                      of the Block containing the requested location.
 * \param       uncompressed_offset
 *                      The actual uncompressed offset of the beginning of
 *                      the Block. uncompressed_offset <= uncompressed_target
 *                      is always true; the application needs to uncompress
 *                      uncompressed_target - uncompressed_offset bytes to
 *                      reach the requested target offset.
 * \param       total_size
 *                      Total Size of the Block. If the Index is incomplete,
 *                      this may be LZMA_VLI_VALUE_UNKNOWN indicating unknown
 *                      size.
 * \param       uncompressed_size
 *                      Uncompressed Size of the Block. If the Index is
 *                      incomplete, this may be LZMA_VLI_VALUE_UNKNOWN
 *                      indicating unknown size. The application must pass
 *                      this value to the Block decoder to verify FIXME
 *
 * \return
 *
 * \note        This function is currently implemented as a linear search.
 *              If there are many Index Records, this can be really slow.
 *              This can be improved in newer liblzma versions if needed.
 */
extern lzma_bool lzma_info_data_locate(const lzma_info *info,
		lzma_vli uncompressed_target,
		lzma_vli *lzma_restrict stream_offset,
		lzma_vli *lzma_restrict uncompressed_offset,
		lzma_vli *lzma_restrict total_size,
		lzma_vli *lzma_restrict uncompressed_size);
