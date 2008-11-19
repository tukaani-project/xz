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

#ifndef MAIN_H
#define MAIN_H

/// Possible exit status values. These are the same as used by gzip and bzip2.
enum exit_status_type {
	E_SUCCESS  = 0,
	E_ERROR    = 1,
	E_WARNING  = 2,
};


/// If this is true, we will clean up the possibly incomplete output file,
/// return to main() as soon as practical. That is, the code needs to poll
/// this variable in various places.
extern volatile sig_atomic_t user_abort;


/// Block the signals which don't have SA_RESTART and which would just set
/// user_abort to true. This is handy when we don't want to handle EINTR
/// and don't want SA_RESTART either.
extern void signals_block(void);


/// Unblock the signals blocked by signals_block().
extern void signals_unblock(void);


/// Sets the exit status after a warning or error has occurred. If new_status
/// is EX_WARNING and the old exit status was already EX_ERROR, the exit
/// status is not changed.
extern void set_exit_status(enum exit_status_type new_status);


/// Exits the program using the given status. This takes care of closing
/// stdin, stdout, and stderr and catches possible errors. If we had got
/// a signal, this function will raise it so that to the parent process it
/// appears that we were killed by the signal sent by the user.
extern void my_exit(enum exit_status_type status) lzma_attribute((noreturn));


#endif
