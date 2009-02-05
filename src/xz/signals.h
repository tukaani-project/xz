///////////////////////////////////////////////////////////////////////////////
//
/// \file       signals.h
/// \brief      Handling signals to abort operation
//
//  Copyright (C) 2007-2009 Lasse Collin
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

/// If this is true, we will clean up the possibly incomplete output file,
/// return to main() as soon as practical. That is, the code needs to poll
/// this variable in various places.
extern volatile sig_atomic_t user_abort;


/// Initialize the signal handler, which will set user_abort to true when
/// user e.g. presses C-c.
extern void signals_init(void);


#ifndef _WIN32

/// Block the signals which don't have SA_RESTART and which would just set
/// user_abort to true. This is handy when we don't want to handle EINTR
/// and don't want SA_RESTART either.
extern void signals_block(void);

/// Unblock the signals blocked by signals_block().
extern void signals_unblock(void);

/// If user has sent us a signal earlier to terminate the process,
/// re-raise that signal to actually terminate the process.
extern void signals_exit(void);

#else

#define signals_block() do { } while (0)
#define signals_unblock() do { } while (0)
#define signals_exit() do { } while (0)

#endif
