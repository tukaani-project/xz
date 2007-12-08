///////////////////////////////////////////////////////////////////////////////
//
/// \file       error.c
/// \brief      Error message printing
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

#ifndef ERROR_H
#define ERROR_H

#include "private.h"


typedef enum {
	SUCCESS           = 0,
	ERROR             = 1,
	WARNING           = 2,
} exit_status_type;


typedef enum {
	V_SILENT,
	V_ERROR,
	V_WARNING,
	V_VERBOSE,
	V_DEBUG,
} verbosity_type;


extern exit_status_type exit_status;

extern verbosity_type verbosity;

/// Like GNU's program_invocation_name but portable
extern char *argv0;

/// Once this is non-zero, all threads must shutdown and clean up incomplete
/// output files from the disk.
extern volatile sig_atomic_t user_abort;


extern const char * str_strm_error(lzma_ret code);

extern void errmsg(verbosity_type v, const char *fmt, ...)
		lzma_attribute((format(printf, 2, 3)));

extern void set_exit_status(exit_status_type new_status);

extern void my_exit(int status) lzma_attribute((noreturn));

extern void out_of_memory(void);

extern void internal_error(void);

#endif
