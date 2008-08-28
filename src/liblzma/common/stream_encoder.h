///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_encoder.h
/// \brief      Encodes .lzma Streams
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

#ifndef LZMA_STREAM_ENCODER_H
#define LZMA_STREAM_ENCODER_H

#include "common.h"


extern lzma_ret lzma_stream_encoder_init(
		lzma_next_coder *next, lzma_allocator *allocator,
		const lzma_filter *filters, lzma_check check);

#endif
