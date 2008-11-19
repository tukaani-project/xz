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

/// \brief      Safe malloc() that never returns NULL
///
/// \note       xmalloc(), xrealloc(), and xstrdup() must not be used when
///             there are files open for writing, that should be cleaned up
///             before exiting.
#define xmalloc(size) xrealloc(NULL, size)


/// \brief      Safe realloc() that never returns NULL
extern void *xrealloc(void *ptr, size_t size);


/// \brief      Safe strdup() that never returns NULL
extern char *xstrdup(const char *src);


/// \brief      Fancy version of strtoull()
///
/// \param      name    Name of the option to show in case of an error
/// \param      value   String containing the number to be parsed; may
///                     contain suffixes "k", "M", "G", "Ki", "Mi", or "Gi"
/// \param      min     Minimum valid value
/// \param      max     Maximum valid value
///
/// \return     Parsed value that is in the range [min, max]. Does not return
///             if an error occurs.
///
extern uint64_t str_to_uint64(const char *name, const char *value,
		uint64_t min, uint64_t max);


/// \brief      Check if filename is empty and print an error message
extern bool is_empty_filename(const char *filename);


/// \brief      Test if stdin is a terminal
///
/// If stdin is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdin(void);


/// \brief      Test if stdout is a terminal
///
/// If stdout is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdout(void);

#endif
