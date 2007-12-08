///////////////////////////////////////////////////////////////////////////////
//
/// \file       version.c
/// \brief      liblzma version number
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

#include "common.h"


LZMA_API const uint32_t lzma_version_number = LZMA_VERSION;

LZMA_API const char *const lzma_version_string = PACKAGE_VERSION;
