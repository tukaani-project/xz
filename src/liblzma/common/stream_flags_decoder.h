///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_flags_decoder.h
/// \brief      Decodes Stream Header and Footer from .lzma files
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

#ifndef LZMA_STREAM_FLAGS_DECODER_H
#define LZMA_STREAM_FLAGS_DECODER_H

#include "common.h"

extern lzma_ret lzma_stream_header_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options);

extern lzma_ret lzma_stream_tail_decoder_init(lzma_next_coder *next,
		lzma_allocator *allocator, lzma_stream_flags *options);

#endif
