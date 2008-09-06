///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzmadec.c
/// \brief      Simple single-threaded tool to uncompress .lzma files
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

#include "sysdefs.h"

#ifdef HAVE_ERRNO_H
#	include <errno.h>
#else
extern int errno;
#endif

#include <stdio.h>
#include <unistd.h>

#ifdef WIN32
#	include <fcntl.h>
#endif

#include "getopt.h"
#include "physmem.h"


enum return_code {
	SUCCESS,
	ERROR,
	WARNING,
};


enum format_type {
	FORMAT_AUTO,
	FORMAT_NATIVE,
	FORMAT_ALONE,
};


enum {
	OPTION_FORMAT = INT_MIN,
};


/// Input buffer
static uint8_t in_buf[BUFSIZ];

/// Output buffer
static uint8_t out_buf[BUFSIZ];

/// Decoder
static lzma_stream strm = LZMA_STREAM_INIT;

/// Number of bytes to use memory at maximum
static size_t memlimit;

/// Program name to be shown in error messages
static const char *argv0;

/// File currently being processed
static FILE *file;

/// Name of the file currently being processed
static const char *filename;

static enum return_code exit_status = SUCCESS;

static enum format_type format_type = FORMAT_AUTO;

static bool force = false;


static void lzma_attribute((noreturn))
help(void)
{
	printf(
"Usage: %s [OPTION]... [FILE]...\n"
"Uncompress files in the .lzma format to the standard output.\n"
"\n"
"  -c, --stdout       (ignored)\n"
"  -d, --decompress   (ignored)\n"
"  -k, --keep         (ignored)\n"
"  -f, --force        allow reading compressed data from a terminal\n"
"  -M, --memory=NUM   use NUM bytes of memory at maximum; the suffixes\n"
"                     k, M, G, Ki, Mi, and Gi are supported.\n"
"      --format=FMT   accept only files in the given file format;\n"
"                     possible FMTs are `auto', `native', and alone',\n"
"  -h, --help         display this help and exit\n"
"  -V, --version      display version and license information and exit\n"
"\n"
"With no FILE, or when FILE is -, read standard input.\n"
"\n"
"On this configuration, the tool will use about %" PRIu64
		" MiB of memory at maximum.\n"
"\n"
"Report bugs to <" PACKAGE_BUGREPORT "> (in English or Finnish).\n",
		argv0, ((uint64_t)(memlimit) + 512 * 1024) / (1024 * 1024));
		// Using PRIu64 above instead of %zu to support pre-C99 libc.
	exit(0);
}


static void lzma_attribute((noreturn))
version(void)
{
	printf(
"lzmadec (LZMA Utils) " PACKAGE_VERSION "\n"
"\n"
"Copyright (C) 1999-2006 Igor Pavlov\n"
"Copyright (C) 2007 Lasse Collin\n"
"\n"
"This program is free software; you can redistribute it and/or\n"
"modify it under the terms of the GNU Lesser General Public\n"
"License as published by the Free Software Foundation; either\n"
"version 2.1 of the License, or (at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"Lesser General Public License for more details.\n"
"\n");
	exit(0);
}


/// Finds out the amount of physical memory in the system, and sets
/// a default memory usage limit.
static void
set_default_memlimit(void)
{
	uint64_t mem = physmem();
	if (mem != 0) {
		mem /= 3;

#if UINT64_MAX > SIZE_MAX
		if (mem > SIZE_MAX)
			mem = SIZE_MAX;
#endif

		memlimit = mem / 3;
	} else {
		// Cannot autodetect, use 10 MiB as the default limit.
		memlimit = (1U << 23) + (1U << 21);
	}

	return;
}


/// \brief      Converts a string to size_t
///
/// This is rudely copied from src/lzma/util.c and modified a little. :-(
///
static size_t
str_to_size(const char *value)
{
	size_t result = 0;

	if (*value < '0' || *value > '9') {
		fprintf(stderr, "%s: %s: Not a number", argv0, value);
		exit(ERROR);
	}

	do {
		// Don't overflow.
		if (result > (SIZE_MAX - 9) / 10)
			return SIZE_MAX;

		result *= 10;
		result += *value - '0';
		++value;
	} while (*value >= '0' && *value <= '9');

	if (*value != '\0') {
		// Look for suffix.
		static const struct {
			const char name[4];
			size_t multiplier;
		} suffixes[] = {
			{ "k",   1000 },
			{ "kB",  1000 },
			{ "M",   1000000 },
			{ "MB",  1000000 },
			{ "G",   1000000000 },
			{ "GB",  1000000000 },
			{ "Ki",  1024 },
			{ "KiB", 1024 },
			{ "Mi",  1048576 },
			{ "MiB", 1048576 },
			{ "Gi",  1073741824 },
			{ "GiB", 1073741824 }
		};

		size_t multiplier = 0;
		for (size_t i = 0; i < ARRAY_SIZE(suffixes); ++i) {
			if (strcmp(value, suffixes[i].name) == 0) {
				multiplier = suffixes[i].multiplier;
				break;
			}
		}

		if (multiplier == 0) {
			fprintf(stderr, "%s: %s: Invalid suffix",
					argv0, value);
			exit(ERROR);
		}

		// Don't overflow here either.
		if (result > SIZE_MAX / multiplier)
			result = SIZE_MAX;
		else
			result *= multiplier;
	}

	return result;
}


/// Parses command line options.
static void
parse_options(int argc, char **argv)
{
	static const char short_opts[] = "cdkfM:hV";
	static const struct option long_opts[] = {
		{ "stdout",       no_argument,         NULL, 'c' },
		{ "to-stdout",    no_argument,         NULL, 'c' },
		{ "decompress",   no_argument,         NULL, 'd' },
		{ "uncompress",   no_argument,         NULL, 'd' },
		{ "force",        no_argument,         NULL, 'f' },
		{ "keep",         no_argument,         NULL, 'k' },
		{ "memory",       required_argument,   NULL, 'M' },
		{ "format",       required_argument,   NULL, OPTION_FORMAT },
		{ "help",         no_argument,         NULL, 'h' },
		{ "version",      no_argument,         NULL, 'V' },
		{ NULL,           0,                   NULL, 0   }
	};

	int c;

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL))
			!= -1) {
		switch (c) {
		case 'c':
		case 'd':
		case 'k':
			break;

		case 'f':
			force = true;
			break;

		case 'M':
			memlimit = str_to_size(optarg);
			break;

		case 'h':
			help();

		case 'V':
			version();

		case OPTION_FORMAT: {
			if (strcmp("auto", optarg) == 0) {
				format_type = FORMAT_AUTO;
			} else if (strcmp("native", optarg) == 0) {
				format_type = FORMAT_NATIVE;
			} else if (strcmp("alone", optarg) == 0) {
				format_type = FORMAT_ALONE;
			} else {
				fprintf(stderr, "%s: %s: Unknown file format "
						"name\n", argv0, optarg);
				exit(ERROR);
			}
			break;
		}

		default:
			exit(ERROR);
		}
	}

	return;
}


/// Initializes lzma_stream structure for decoding of a new Stream.
static void
init(void)
{
	const uint32_t flags = LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED;
	lzma_ret ret;

	switch (format_type) {
	case FORMAT_AUTO:
		ret = lzma_auto_decoder(&strm, memlimit, flags);
		break;

	case FORMAT_NATIVE:
		ret = lzma_stream_decoder(&strm, memlimit, flags);
		break;

	case FORMAT_ALONE:
		ret = lzma_alone_decoder(&strm, memlimit);
		break;

	default:
		assert(0);
		ret = LZMA_PROG_ERROR;
	}

	if (ret != LZMA_OK) {
		fprintf(stderr, "%s: ", argv0);

		if (ret == LZMA_MEM_ERROR)
			fprintf(stderr, "%s\n", strerror(ENOMEM));
		else
			fprintf(stderr, "Internal program error (bug)\n");

		exit(ERROR);
	}

	return;
}


static void
uncompress(void)
{
	if (file == stdin && !force && isatty(STDIN_FILENO)) {
		fprintf(stderr, "%s: Compressed data not read from "
				"a terminal.\n%s: Use `-f' to force reading "
				"from a terminal, or `-h' for help.\n",
				argv0, argv0);
		exit(ERROR);
	}

	init();

	strm.avail_in = 0;
	strm.next_out = out_buf;
	strm.avail_out = BUFSIZ;

	lzma_action action = LZMA_RUN;

	while (true) {
		if (strm.avail_in == 0) {
			strm.next_in = in_buf;
			strm.avail_in = fread(in_buf, 1, BUFSIZ, file);

			if (ferror(file)) {
				// POSIX says that fread() sets errno if
				// an error occurred. ferror() doesn't
				// touch errno.
				fprintf(stderr, "%s: %s: Error reading "
						"input file: %s\n",
						argv0, filename,
						strerror(errno));
				exit(ERROR);
			}

			if (feof(file))
				action = LZMA_FINISH;
		}

		const lzma_ret ret = lzma_code(&strm, action);

		// Write and check write error before checking decoder error.
		// This way as much data as possible gets written to output
		// even if decoder detected an error.
		if (strm.avail_out == 0 || ret != LZMA_OK) {
			const size_t write_size = BUFSIZ - strm.avail_out;

			if (fwrite(out_buf, 1, write_size, stdout)
					!= write_size) {
				// Wouldn't be a surprise if writing to stderr
				// would fail too but at least try to show an
				// error message.
				fprintf(stderr, "%s: Cannot write to "
						"standard output: %s\n", argv0,
						strerror(errno));
				exit(ERROR);
			}

			strm.next_out = out_buf;
			strm.avail_out = BUFSIZ;
		}

		if (ret != LZMA_OK) {
			// FIXME !!! Doesn't work with LZMA_Alone for the
			// same reason as in process.c.
			if (ret == LZMA_STREAM_END)
				return;

			fprintf(stderr, "%s: %s: ", argv0, filename);

			// FIXME Add LZMA_*_CHECK and LZMA_FORMAT_ERROR.
			switch (ret) {
			case LZMA_DATA_ERROR:
				fprintf(stderr, "File is corrupt\n");
				exit(ERROR);

			case LZMA_HEADER_ERROR:
				fprintf(stderr, "Unsupported file "
						"format or filters\n");
				exit(ERROR);

			case LZMA_MEM_ERROR:
				fprintf(stderr, "%s\n", strerror(ENOMEM));
				exit(ERROR);

			case LZMA_MEMLIMIT_ERROR:
				fprintf(stderr, "Memory usage limit "
						"reached\n");
				exit(ERROR);

			case LZMA_BUF_ERROR:
				fprintf(stderr, "Unexpected end of input\n");
				exit(ERROR);

			case LZMA_UNSUPPORTED_CHECK:
				fprintf(stderr, "Unsupported type of "
						"integrity check; not "
						"verifying file integrity\n");
				exit_status = WARNING;
				break;

			case LZMA_PROG_ERROR:
			default:
				fprintf(stderr, "Internal program "
						"error (bug)\n");
				exit(ERROR);
			}
		}
	}
}


int
main(int argc, char **argv)
{
	argv0 = argv[0];

	set_default_memlimit();

	parse_options(argc, argv);

	lzma_init_decoder();

#ifdef WIN32
	setmode(fileno(stdin), O_BINARY);
	setmode(fileno(stdout), O_BINARY);
#endif

	if (optind == argc) {
		file = stdin;
		filename = "(stdin)";
		uncompress();
	} else {
		do {
			if (strcmp(argv[optind], "-") == 0) {
				file = stdin;
				filename = "(stdin)";
				uncompress();
			} else {
				filename = argv[optind];
				file = fopen(filename, "rb");
				if (file == NULL) {
					fprintf(stderr, "%s: %s: %s\n",
							argv0, filename,
							strerror(errno));
					exit(ERROR);
				}

				uncompress();
				fclose(file);
			}
		} while (++optind < argc);
	}

#ifndef NDEBUG
	// Free the memory only when debugging. Freeing wastes some time,
	// but allows detecting possible memory leaks with Valgrind.
	lzma_end(&strm);
#endif

	return exit_status;
}
