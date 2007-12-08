///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.c
/// \brief      Detection of available hardware resources
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

#ifndef HARDWARE_H
#define HARDWARE_H

#include "private.h"


extern size_t opt_threads;
extern size_t opt_memory;

extern void hardware_init(void);

#endif
