///////////////////////////////////////////////////////////////////////////////
//
/// \file       help.h
/// \brief      Help messages
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

#ifndef HELP_H
#define HELP_H

#include "private.h"


extern void show_try_help(void);

extern void show_help(void) lzma_attribute((noreturn));

extern void show_version(void) lzma_attribute((noreturn));

#endif
