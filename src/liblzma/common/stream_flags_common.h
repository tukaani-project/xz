///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_flags_common.h
/// \brief      Common stuff for Stream flags coders
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

#ifndef LZMA_STREAM_FLAGS_COMMON_H
#define LZMA_STREAM_FLAGS_COMMON_H

#include "common.h"

/// Size of the Stream Flags field
#define LZMA_STREAM_FLAGS_SIZE 2

extern const uint8_t lzma_header_magic[6];
extern const uint8_t lzma_footer_magic[2];

#endif
