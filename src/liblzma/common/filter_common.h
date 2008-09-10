///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_common.c
/// \brief      Filter-specific stuff common for both encoder and decoder
//
//  Copyright (C) 2008 Lasse Collin
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

#ifndef LZMA_FILTER_COMMON_H
#define LZMA_FILTER_COMMON_H

#include "common.h"


/// Both lzma_filter_encoder and lzma_filter_decoder begin with these members.
typedef struct {
	/// Filter ID
	lzma_vli id;

	/// Initializes the filter encoder and calls lzma_next_filter_init()
	/// for filters + 1.
	lzma_init_function init;

	/// Calculates memory usage of the encoder. If the options are
	/// invalid, UINT64_MAX is returned.
	uint64_t (*memusage)(const void *options);

} lzma_filter_coder;


typedef const lzma_filter_coder *(*lzma_filter_find)(lzma_vli id);


extern lzma_ret lzma_raw_coder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *filters,
		lzma_filter_find coder_find, bool is_encoder);


extern uint64_t lzma_memusage_coder(lzma_filter_find coder_find,
		const lzma_filter *filters);


#endif
