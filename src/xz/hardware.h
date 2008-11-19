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


/// Initialize some hardware-specific variables, which are needed by other
/// hardware_* functions.
extern void hardware_init(void);


/// Set custom memory usage limit. This is used for both encoding and
/// decoding. Zero indicates resetting the limit back to defaults.
extern void hardware_memlimit_set(uint64_t memlimit);

/// Get the memory usage limit for encoding. By default this is 90 % of RAM.
extern uint64_t hardware_memlimit_encoder(void);


/// Get the memory usage limit for decoding. By default this is 30 % of RAM.
extern uint64_t hardware_memlimit_decoder(void);

#endif
