///////////////////////////////////////////////////////////////////////////////
//
/// \file       stream_common.c
/// \brief      Common stuff for Stream coders
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

#include "stream_common.h"

const uint8_t lzma_header_magic[6] = { 0xFF, 0x4C, 0x5A, 0x4D, 0x41, 0x00 };
const uint8_t lzma_footer_magic[2] = { 0x59, 0x5A };
