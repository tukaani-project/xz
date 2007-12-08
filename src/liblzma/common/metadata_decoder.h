///////////////////////////////////////////////////////////////////////////////
//
/// \file       metadata_decoder.h
/// \brief      Decodes metadata stored in Metadata Blocks
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

#ifndef LZMA_METADATA_DECODER_H
#define LZMA_METADATA_DECODER_H

#include "common.h"


extern lzma_ret lzma_metadata_decoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, lzma_metadata *metadata,
		bool want_extra);

#endif
