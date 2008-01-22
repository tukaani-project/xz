///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_common.c
/// \brief      Shared stuff for easy encoder initialization functions
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

#include "common.h"

#ifndef LZMA_EASY_COMMON_H
#define LZMA_EASY_COMMON_H

extern bool lzma_easy_set_filters(
		lzma_options_filter *filters, uint32_t level);

#endif
