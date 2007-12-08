///////////////////////////////////////////////////////////////////////////////
//
/// \file       subblock_decoder_helper.h
/// \brief      Helper filter for the Subblock decoder
///
/// This filter is used to indicate End of Input for subfilters needing it.
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

#ifndef LZMA_SUBBLOCK_DECODER_HELPER_H
#define LZMA_SUBBLOCK_DECODER_HELPER_H

#include "common.h"


typedef struct {
	bool end_was_reached;
} lzma_options_subblock_helper;


extern lzma_ret lzma_subblock_decoder_helper_init(lzma_next_coder *next,
		lzma_allocator *allocator, const lzma_filter_info *filters);

#endif
