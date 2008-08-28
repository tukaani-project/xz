///////////////////////////////////////////////////////////////////////////////
//
/// \file       simple_encoder.c
/// \brief      Properties encoder for simple filters
//
//  Copyright (C) 2007-2008 Lasse Collin
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

#ifndef LZMA_SIMPLE_ENCODER_H
#define LZMA_SIMPLE_ENCODER_H

#include "simple_coder.h"


extern lzma_ret lzma_simple_props_size(uint32_t *size, const void *options);

extern lzma_ret lzma_simple_props_encode(const void *options, uint8_t *out);

#endif
