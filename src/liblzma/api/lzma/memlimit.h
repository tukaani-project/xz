/**
 * \file        lzma/memlimit.h
 * \brief       Memory usage limitter
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
 * \brief       Opaque data type used with the memory usage limitting functions
 */
typedef struct lzma_memlimit_s lzma_memlimit;


/**
 * \brief       Allocates and initializes a new lzma_memlimit structure
 *
 * It is easy to make liblzma to use huge amounts of memory. This can
 * be a problem especially with the decoder, since it a file requiring
 * huge amounts of memory to uncompress could allow even a denial of
 * service attack if the memory usage wasn't limited.
 *
 * liblzma provides a set of functions to control memory usage. Pointers
 * to these functions can be used in lzma_allocator structure, which makes
 * it easy to limit memory usage with liblzma.
 *
 * The memory limitter functions are not tied to limitting memory usage
 * with liblzma itself. You can use them with anything you like.
 *
 * In multi-threaded applications, only one thread at once may use the same
 * lzma_memlimit structure. If there is a need, this limitation may
 * be removed in future versions without breaking the libary API/ABI.
 *
 * \param       limit   Initial memory usage limit in bytes
 *
 * \return      Pointer to allocated and initialized lzma_memlimit
 *              structure. On error, NULL is returned. The reason behind
 *              an error is either that malloc() failed or that the given
 *              limit was so small that it didn't allow allocating even
 *              the lzma_memlimit structure itself.
 *
 * \note        Excluding lzma_memlimit_usage(), the functions whose name begin
 *              lzma_memlimit_ can be used even if lzma_init() hasn't been
 *              called.
 */
extern lzma_memlimit *lzma_memlimit_create(size_t limit);


/**
 * \brief       Sets a new memory usage limit
 *
 * \param       mem     Pointer to a lzma_memlimit structure returned
 *                      earlier by lzma_memry_limit_create().
 * \param       limit   New memory usage limit
 *
 * The new usage limit may be smaller than the amount of memory currently
 * allocated via *mem: New allocations will fail until enough memory has
 * been freed or a new limit is set, but the existing allocatations will
 * stay untouched.
 */
extern void lzma_memlimit_set(lzma_memlimit *mem, size_t limit);


/**
 * \brief       Gets the current memory usage limit
 */
extern size_t lzma_memlimit_get(const lzma_memlimit *mem);


/**
 * \brief       Gets the amount of currently allocated memory
 *
 * \note        This value includes the sizes of some helper structures,
 *              thus it will always be larger than the total number of
 *              bytes allocated via lzma_memlimit_alloc().
 */
extern size_t lzma_memlimit_used(const lzma_memlimit *mem);


/**
 * \brief       Gets the maximum amount of memory required in total
 *
 * Returns how much memory was or would have been allocated at the same time.
 * If lzma_memlimit_alloc() was requested so much memory that the limit
 * would have been exceeded or malloc() simply ran out of memory, the
 * requested amount is still included to the value returned by
 * lzma_memlimit_max(). This may be used as a hint how much bigger memory
 * limit would have been needed.
 *
 * If the clear flag is set, the internal variable holding the maximum
 * value is set to the current memory usage (the same value as returned
 * by lzma_memlimit_used()).
 *
 * \note        Usually liblzma needs to allocate many chunks of memory, and
 *              displaying a message like "memory usage limit reached, at
 *              least 1024 bytes would have been needed" may be confusing,
 *              because the next allocation could have been e.g. 8 MiB.
 *
 * \todo        The description of this function is unclear.
 */
extern size_t lzma_memlimit_max(lzma_memlimit *mem, lzma_bool clear);


/**
 * \brief       Checks if memory limit was reached at some point
 *
 * This function is useful to find out if the reason for LZMA_MEM_ERROR
 * was running out of memory or hitting the memory usage limit imposed
 * by lzma_memlimit_alloc(). If the clear argument is true, the internal
 * flag, that indicates that limit was reached, is cleared.
 */
extern lzma_bool lzma_memlimit_reached(lzma_memlimit *mem, lzma_bool clear);


/**
 * \brief       Gets the number of allocations owned by the memory limitter
 *
 * The count does not include the helper structures; if no memory has
 * been allocated with lzma_memlimit_alloc() or all memory allocated
 * has been freed or detached, this will return zero.
 */
extern size_t lzma_memlimit_count(const lzma_memlimit *mem);


/**
 * \brief       Allocates memory with malloc() if memory limit allows
 *
 * \param       mem     Pointer to a lzma_memlimit structure returned
 *                      earlier by lzma_memry_limit_create().
 * \param       nmemb   Number of elements to allocate. While liblzma always
 *                      sets this to one, this function still takes the
 *                      value of nmemb into account to keep the function
 *                      usable with zlib and libbzip2.
 * \param       size    Size of an element.
 *
 * \return      Pointer to memory allocated with malloc(nmemb * size),
 *              except if nmemb * size == 0 which returns malloc(1).
 *              On error, NULL is returned.
 *
 * \note        This function assumes that nmemb * size is at maximum of
 *              SIZE_MAX. If it isn't, an overflow will occur resulting
 *              invalid amount of memory being allocated.
 */
extern void *lzma_memlimit_alloc(
		lzma_memlimit *mem, size_t nmemb, size_t size);


/**
 * \brief       Removes the pointer from memory limitting list
 *
 * \param       mem     Pointer to a lzma_memlimit structure returned
 *                      earlier by lzma_memry_limit_create().
 * \param       ptr     Pointer returned earlier by lzma_memlimit_alloc().
 *
 * This function removes ptr from the internal list and decreases the
 * counter of used memory accordingly. The ptr itself isn't freed. This is
 * useful when Extra Records allocated by liblzma using lzma_memlimit
 * are needed by the application and must not be freed when the
 * lzma_memlimit structure is destroyed.
 *
 * It is OK to call this function with ptr that hasn't been allocated with
 * lzma_memlimit_alloc(). In that case, this has no effect other than wasting
 * a few CPU cycles.
 */
extern void lzma_memlimit_detach(lzma_memlimit *mem, void *ptr);


/**
 * \brief       Frees memory and updates the memory limit list
 *
 * This is like lzma_memlimit_detach() but also frees the given pointer.
 */
extern void lzma_memlimit_free(lzma_memlimit *mem, void *ptr);


/**
 * \brief       Frees the memory allocated for and by the memory usage limitter
 *
 * \param       mem             Pointer to memory limitter
 * \param       free_allocated  If this is non-zero, all the memory allocated
 *                              by lzma_memlimit_alloc() using *mem is also
 *                              freed if it hasn't already been freed with
 *                              lzma_memlimit_free(). Usually this should be
 *                              set to true.
 */
extern void lzma_memlimit_end(
		lzma_memlimit *mem, lzma_bool free_allocated);
