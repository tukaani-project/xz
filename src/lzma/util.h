///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.h
/// \brief      Miscellaneous utility functions
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef UTIL_H
#define UTIL_H

#include "private.h"

extern uint64_t str_to_uint64(const char *name, const char *value,
		uint64_t min, uint64_t max);

extern const char *str_filename(const char *filename);

extern bool is_empty_filename(const char *filename);

#endif
