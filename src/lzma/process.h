///////////////////////////////////////////////////////////////////////////////
//
/// \file       process.c
/// \brief      Compresses or uncompresses a file
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

#ifndef PROCESS_H
#define PROCESS_H

#include "private.h"


enum operation_mode {
	MODE_COMPRESS,
	MODE_DECOMPRESS,
	MODE_TEST,
	MODE_LIST,
};


// NOTE: The order of these is significant in suffix.c.
enum format_type {
	FORMAT_AUTO,
	FORMAT_XZ,
	FORMAT_LZMA,
	// HEADER_GZIP,
	FORMAT_RAW,
};


/// Operation mode of the command line tool. This is set in args.c and read
/// in several files.
extern enum operation_mode opt_mode;

/// File format to use when encoding or what format(s) to accept when
/// decoding. This is a global because it's needed also in suffix.c.
/// This is set in args.c.
extern enum format_type opt_format;


/// Set the integrity check type used when compressing
extern void coder_set_check(lzma_check check);

/// Set preset number
extern void coder_set_preset(size_t new_preset);

/// Add a filter to the custom filter chain
extern void coder_add_filter(lzma_vli id, void *options);

///
extern void coder_set_compression_settings(void);

extern void process_init(void);

extern void process_file(const char *filename);

#endif
