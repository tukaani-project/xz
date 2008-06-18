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

#include "private.h"
#include <stdarg.h>


exit_status_type exit_status = SUCCESS;
verbosity_type verbosity = V_WARNING;
char *argv0 = NULL;
volatile sig_atomic_t user_abort = 0;


extern const char *
str_strm_error(lzma_ret code)
{
	switch (code) {
	case LZMA_OK:
		return _("Operation successful");

	case LZMA_STREAM_END:
		return _("Operation finished successfully");

	case LZMA_PROG_ERROR:
		return _("Internal error (bug)");

	case LZMA_DATA_ERROR:
		return _("Compressed data is corrupt");

	case LZMA_MEM_ERROR:
		return strerror(ENOMEM);

	case LZMA_BUF_ERROR:
		return _("Unexpected end of input");

	case LZMA_HEADER_ERROR:
		return _("Unsupported options");

	case LZMA_UNSUPPORTED_CHECK:
		return _("Unsupported integrity check type");

	case LZMA_MEMLIMIT_ERROR:
		return _("Memory usage limit reached");

	case LZMA_FORMAT_ERROR:
		return _("File format not recognized");

	default:
		return NULL;
	}
}


extern void
set_exit_status(exit_status_type new_status)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);

	if (new_status != WARNING || exit_status == SUCCESS)
		exit_status = new_status;

	pthread_mutex_unlock(&mutex);
	return;
}


extern void lzma_attribute((noreturn))
my_exit(int status)
{
	// Close stdout. If something goes wrong, print an error message
	// to stderr.
	{
		const int ferror_err = ferror(stdout);
		const int fclose_err = fclose(stdout);
		if (fclose_err) {
			errmsg(V_ERROR, _("Writing to standard output "
					"failed: %s"), strerror(errno));
			status = ERROR;
		} else if (ferror_err) {
			// Some error has occurred but we have no clue about
			// the reason since fclose() succeeded.
			errmsg(V_ERROR, _("Writing to standard output "
					"failed: %s"), "Unknown error");
			status = ERROR;
		}
	}

	// Close stderr. If something goes wrong, there's nothing where we
	// could print an error message. Just set the exit status.
	{
		const int ferror_err = ferror(stderr);
		const int fclose_err = fclose(stderr);
		if (fclose_err || ferror_err)
			status = ERROR;
	}

	exit(status);
}


extern void lzma_attribute((format(printf, 2, 3)))
errmsg(verbosity_type v, const char *fmt, ...)
{
	va_list ap;

	if (v <= verbosity) {
		va_start(ap, fmt);

		static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_lock(&mutex);

		fprintf(stderr, "%s: ", argv0);
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");

		pthread_mutex_unlock(&mutex);

		va_end(ap);
	}

	if (v == V_ERROR)
		set_exit_status(ERROR);
	else if (v == V_WARNING)
		set_exit_status(WARNING);

	return;
}


extern void
out_of_memory(void)
{
	errmsg(V_ERROR, "%s", strerror(ENOMEM));
	user_abort = 1;
	return;
}


extern void
internal_error(void)
{
	errmsg(V_ERROR, _("Internal error (bug)"));
	user_abort = 1;
	return;
}
