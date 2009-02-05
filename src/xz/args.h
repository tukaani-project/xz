///////////////////////////////////////////////////////////////////////////////
//
/// \file       args.h
/// \brief      Argument parsing
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

typedef struct {
	/// Filenames from command line
	char **arg_names;

	/// Number of filenames from command line
	size_t arg_count;

	/// Name of the file from which to read filenames. This is NULL
	/// if --files or --files0 was not used.
	char *files_name;

	/// File opened for reading from which filenames are read. This is
	/// non-NULL only if files_name is non-NULL.
	FILE *files_file;

	/// Delimiter for filenames read from files_file
	char files_delim;

} args_info;


extern bool opt_stdout;
extern bool opt_force;
extern bool opt_keep_original;
// extern bool opt_recursive;

extern const char *stdin_filename;

extern void args_parse(args_info *args, int argc, char **argv);
