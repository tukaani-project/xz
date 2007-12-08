///////////////////////////////////////////////////////////////////////////////
//
/// \file       metadata_encoder.h
/// \brief      Encodes metadata to be stored into Metadata Blocks
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

#ifndef LZMA_METADATA_ENCODER_H
#define LZMA_METADATA_ENCODER_H

#include "common.h"


extern lzma_ret lzma_metadata_encoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		lzma_options_block *options, const lzma_metadata *metadata);

#endif
