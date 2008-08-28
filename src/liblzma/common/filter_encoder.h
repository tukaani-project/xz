///////////////////////////////////////////////////////////////////////////////
//
/// \file       filter_encoder.c
/// \brief      Filter ID mapping to filter-specific functions
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

#ifndef LZMA_FILTER_ENCODER_H
#define LZMA_FILTER_ENCODER_H

#include "common.h"


// FIXME !!! Public API
extern lzma_vli lzma_chunk_size(const lzma_filter *filters);
extern lzma_ret lzma_properties_size(
		uint32_t *size, const lzma_filter *filter);
extern lzma_ret lzma_properties_encode(
		const lzma_filter *filter, uint8_t *props);


extern lzma_ret lzma_raw_encoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *options);

#endif
