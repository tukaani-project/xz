///////////////////////////////////////////////////////////////////////////////
//
/// \file       common.h
/// \brief      Definitions common to the whole liblzma library
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

#ifndef LZMA_COMMON_H
#define LZMA_COMMON_H

#include "../../common/sysdefs.h"

// Don't use ifdef...
#if HAVE_VISIBILITY
#	define LZMA_API __attribute__((__visibility__("default")))
#else
#	define LZMA_API
#endif


/// Size of temporary buffers needed in some filters
#define LZMA_BUFFER_SIZE 4096


/// Internal helper filter used by Subblock decoder. It is mapped to an
/// otherwise invalid Filter ID, which is impossible to get from any input
/// file (even if malicious file).
#define LZMA_FILTER_SUBBLOCK_HELPER (UINT64_MAX - 2)


///////////
// Types //
///////////

typedef struct lzma_coder_s lzma_coder;

typedef struct lzma_next_coder_s lzma_next_coder;

typedef struct lzma_filter_info_s lzma_filter_info;


typedef lzma_ret (*lzma_init_function)(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter_info *filters);

typedef lzma_ret (*lzma_code_function)(
		lzma_coder *coder, lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size,
		lzma_action action);

typedef void (*lzma_end_function)(
		lzma_coder *coder, lzma_allocator *allocator);


/// Hold data and function pointers of the next filter in the chain.
struct lzma_next_coder_s {
	/// Pointer to coder-specific data
	lzma_coder *coder;

	/// "Pointer" to init function. This is never called here.
	/// We need only to detect if we are initializing a coder
	/// that was allocated earlier. See code.c and next_coder.c.
	uintptr_t init;

	/// Pointer to function to do the actual coding
	lzma_code_function code;

	/// Pointer to function to free lzma_next_coder.coder
	lzma_end_function end;
};

#define LZMA_NEXT_CODER_INIT \
	(lzma_next_coder){ \
		.coder = NULL, \
		.init = 0, \
		.code = NULL, \
		.end = NULL, \
	}


struct lzma_internal_s {
	lzma_next_coder next;

	enum {
		ISEQ_RUN,
		ISEQ_SYNC_FLUSH,
		ISEQ_FULL_FLUSH,
		ISEQ_FINISH,
		ISEQ_END,
		ISEQ_ERROR,
	} sequence;

	bool supported_actions[4];
	bool allow_buf_error;
	size_t avail_in;
};


struct lzma_filter_info_s {
	/// Pointer to function used to initialize the filter.
	/// This is NULL to indicate end of array.
	lzma_init_function init;

	/// Pointer to filter's options structure
	void *options;

	/// Uncompressed size of the filter, or LZMA_VLI_VALUE_UNKNOWN
	/// if unknown.
	lzma_vli uncompressed_size;
};


/*
typedef struct {
	lzma_init_function init;
	uint32_t (*input_alignment)(lzma_vli id, const void *options);
	uint32_t (*output_alignment)(lzma_vli id, const void *options);
	bool changes_uncompressed_size;
	bool supports_eopm;
} lzma_filter_hook;
*/


///////////////
// Functions //
///////////////

/// Allocates memory
extern void *lzma_alloc(size_t size, lzma_allocator *allocator)
		lzma_attribute((malloc));

/// Frees memory
extern void lzma_free(void *ptr, lzma_allocator *allocator);

/// Initializes lzma_stream FIXME desc
extern lzma_ret lzma_strm_init(lzma_stream *strm);

///
extern lzma_ret lzma_next_filter_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

///
extern void lzma_next_coder_end(lzma_next_coder *next,
		lzma_allocator *allocator);


extern lzma_ret lzma_filter_flags_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_options_filter *options);

extern lzma_ret lzma_block_header_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_options_block *options);

extern lzma_ret lzma_stream_encoder_single_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_options_stream *options);

extern lzma_ret lzma_stream_decoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		lzma_extra **header, lzma_extra **footer);


/// \brief      Wrapper for memcpy()
///
/// This function copies as much data as possible from in[] to out[] and
/// updates *in_pos and *out_pos accordingly.
///
static inline size_t
bufcpy(const uint8_t *restrict in, size_t *restrict in_pos, size_t in_size,
		uint8_t *restrict out, size_t *restrict out_pos,
		size_t out_size)
{
	const size_t in_avail = in_size - *in_pos;
	const size_t out_avail = out_size - *out_pos;
	const size_t copy_size = MIN(in_avail, out_avail);

	memcpy(out + *out_pos, in + *in_pos, copy_size);

	*in_pos += copy_size;
	*out_pos += copy_size;

	return copy_size;
}


/// \brief      Initializing the next coder
///
/// lzma_next_coder can point to different types of coders. The existing
/// coder may be different than what we are initializing now. In that case
/// we must git rid of the old coder first. Otherwise we reuse the existing
/// coder structure.
///
#define lzma_next_coder_init2(next, allocator, cmpfunc, func, ...) \
do { \
	if ((uintptr_t)(&cmpfunc) != (next)->init) \
		lzma_next_coder_end(next, allocator); \
	const lzma_ret ret = func(next, __VA_ARGS__); \
	if (ret == LZMA_OK) { \
		(next)->init = (uintptr_t)(&cmpfunc); \
		assert((next)->code != NULL); \
		assert((next)->end != NULL); \
	} else { \
		lzma_next_coder_end(next, allocator); \
	} \
	return ret; \
} while (0)

/// \brief      Initializing lzma_next_coder
///
/// Call the initialization function, which must take at least one
/// argument in addition to lzma_next_coder and lzma_allocator.
#define lzma_next_coder_init(func, next, allocator, ...) \
	lzma_next_coder_init2(next, allocator, \
			func, func, allocator, __VA_ARGS__)


/// \brief      Initializing lzma_stream
///
/// lzma_strm initialization with more detailed options.
#define lzma_next_strm_init2(strm, cmpfunc, func, ...) \
do { \
	lzma_ret ret = lzma_strm_init(strm); \
	if (ret != LZMA_OK) \
		return ret; \
	if ((uintptr_t)(&cmpfunc) != (strm)->internal->next.init) \
		lzma_next_coder_end(\
				&(strm)->internal->next, (strm)->allocator); \
	ret = func(&(strm)->internal->next, __VA_ARGS__); \
	if (ret != LZMA_OK) { \
		lzma_end(strm); \
		return ret; \
	} \
	(strm)->internal->next.init = (uintptr_t)(&cmpfunc); \
	assert((strm)->internal->next.code != NULL); \
	assert((strm)->internal->next.end != NULL); \
} while (0)

/// \brief      Initializing lzma_stream
///
/// Call the initialization function, which must take at least one
/// argument in addition to lzma_next_coder and lzma_allocator.
#define lzma_next_strm_init(strm, func, ...) \
	lzma_next_strm_init2(strm, func, func, (strm)->allocator, __VA_ARGS__)


/// \brief      Return if expression doesn't evaluate to LZMA_OK
///
/// There are several situations where we want to return immediatelly
/// with the value of expr if it isn't LZMA_OK. This macro shortens
/// the code a bit.
///
#define return_if_error(expr) \
do { \
	const lzma_ret ret_ = expr; \
	if (ret_ != LZMA_OK) \
		return ret_; \
} while (0)

#endif
