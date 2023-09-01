///////////////////////////////////////////////////////////////////////////////
//
/// \file       private.h
/// \brief      Common includes, definitions, and prototypes
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"
#include "mythread.h"

#include "lzma.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "tuklib_gettext.h"
#include "tuklib_progname.h"
#include "tuklib_exit.h"
#include "tuklib_mbstr.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#endif

#ifdef _MSC_VER
#define fileno _fileno
#endif

#ifndef STDIN_FILENO
#	define STDIN_FILENO (fileno(stdin))
#endif

#ifndef STDOUT_FILENO
#	define STDOUT_FILENO (fileno(stdout))
#endif

#ifndef STDERR_FILENO
#	define STDERR_FILENO (fileno(stderr))
#endif

#if defined(HAVE_CAPSICUM) || defined(HAVE_PLEDGE)
#	define ENABLE_SANDBOX 1
#endif

// Handling SIGTSTP keeps time-keeping for progress indicator correct
// if xz is stopped. It requires use of clock_gettime() as that is
// async-signal safe in POSIX. Require also SIGALRM support since
// on systems where SIGALRM isn't available, progress indicator code
// polls the time and the SIGTSTP handling adds slight overhead to
// that code. Most (all?) systems that have SIGTSTP also have SIGALRM
// so this requirement won't exclude many systems.
#if defined(HAVE_CLOCK_GETTIME) && defined(SIGTSTP) && defined(SIGALRM)
#	define USE_SIGTSTP_HANDLER 1
#endif

#include "main.h"
#include "mytime.h"
#include "coder.h"
#include "message.h"
#include "args.h"
#include "hardware.h"
#include "file_io.h"
#include "options.h"
#include "signals.h"
#include "suffix.h"
#include "util.h"

#ifdef HAVE_DECODERS
#	include "list.h"
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900 // VS2013 or before
#pragma warning(push)
#pragma warning(disable: 4996) // warning _vsnprintf() is unsafe.
// Emulate C99 snprintf() with _snprintf().
static int msvc_snprintf(char *buf, size_t bufsz, const char *fmt, ...)
{
	int len;
	va_list va;
	va_start(va, fmt);
	len = _vsnprintf(buf, bufsz, fmt, va);
	va_end(va);
	if (!buf) {
		// (bufsz > 0) -> Should already crash.
		// (bufsz == 0) -> Return required buffer size.
	} else if (len >= 0 && (size_t)len <= bufsz) {
		if (len == bufsz && bufsz > 0)
			buf[bufsz - 1] = '\0'; // Ensure null termination.
			// len is correct required buffer size in this case.
	} else {
		// _vsnprintf() does not return required buffer size in case of overflow.
		// Recall _vsnprintf() in special way to find out.
		va_start(va, fmt);
		len = _vsnprintf(NULL, 0, fmt, va); // Get required buffer size.
		va_end(va);
		// assert(len == -1 || len > bufsz);
		if (bufsz > 0)
			buf[bufsz - 1] = '\0'; // Ensure null termination.
	}
	return len;
}
#pragma warning(pop)
#define snprintf msvc_snprintf
#endif
