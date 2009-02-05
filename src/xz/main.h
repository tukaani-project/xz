///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.h
/// \brief      Miscellanous declarations
//
//  Copyright (C) 2008 Lasse Collin
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

/// Possible exit status values. These are the same as used by gzip and bzip2.
enum exit_status_type {
	E_SUCCESS  = 0,
	E_ERROR    = 1,
	E_WARNING  = 2,
};


/// Sets the exit status after a warning or error has occurred. If new_status
/// is EX_WARNING and the old exit status was already EX_ERROR, the exit
/// status is not changed.
extern void set_exit_status(enum exit_status_type new_status);


/// Exits the program using the given status. This takes care of closing
/// stdin, stdout, and stderr and catches possible errors. If we had got
/// a signal, this function will raise it so that to the parent process it
/// appears that we were killed by the signal sent by the user.
extern void my_exit(enum exit_status_type status) lzma_attribute((noreturn));
