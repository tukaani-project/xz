///////////////////////////////////////////////////////////////////////////////
//
/// \file       suffix.h
/// \brief      Checks filename suffix and creates the destination filename
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

#ifndef SUFFIX_H
#define SUFFIX_H

/// \brief      Get the name of the destination file
///
/// Depending on the global variable opt_mode, this tries to find a matching
/// counterpart for src_name. If the name can be constructed, it is allocated
/// and returned (caller must free it). On error, a message is printed and
/// NULL is returned.
extern char *suffix_get_dest_name(const char *src_name);


/// \brief      Set a custom filename suffix
///
/// This function calls xstrdup() for the given suffix, thus the caller
/// doesn't need to keep the memory allocated. There can be only one custom
/// suffix, thus if this is called multiple times, the old suffixes are freed
/// and forgotten.
extern void suffix_set(const char *suffix);

#endif
