///////////////////////////////////////////////////////////////////////////////
//
/// \file       io.h
/// \brief      I/O types and functions
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

#ifndef IO_H
#define IO_H

#include "private.h"

#if BUFSIZ <= 1024
#	define IO_BUFFER_SIZE 8192
#else
#	define IO_BUFFER_SIZE BUFSIZ
#endif


typedef struct {
	const char *src_name;
	char *dest_name;

	int dir_fd;
	int src_fd;
	int dest_fd;

	struct stat src_st;
	ino_t dest_ino;

	bool src_eof;
} file_pair;


extern void io_init(void);

extern void io_finish(void);

extern file_pair *io_open(const char *src_name);

extern void io_close(file_pair *pair, bool success);

extern size_t io_read(file_pair *pair, uint8_t *buf, size_t size);

extern int io_write(const file_pair *pair, const uint8_t *buf, size_t size);


#endif
