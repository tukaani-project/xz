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

#ifndef STDIN_FILENO
#ifdef _MSC_VER
#	define STDIN_FILENO (_fileno(stdin))
#else
#	define STDIN_FILENO (fileno(stdin))
#endif
#endif

#ifndef STDOUT_FILENO
#ifdef _MSC_VER
#	define STDOUT_FILENO (_fileno(stdout))
#else
#	define STDOUT_FILENO (fileno(stdout))
#endif
#endif

#ifndef STDERR_FILENO
#ifdef _MSC_VER
#	define STDERR_FILENO (_fileno(stderr))
#else
#	define STDERR_FILENO (fileno(stderr))
#endif
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

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif
