// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_gettext.h
/// \brief      Wrapper for gettext and friends
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TUKLIB_GETTEXT_H
#define TUKLIB_GETTEXT_H

#include "tuklib_common.h"
#include <locale.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
	// To use UTF-8 code page on Windows 10 version 1903 and later, the
	// active code page has to be set to UTF-8 in the application manifest
	// and UCRT-specific setlocale(LC_ALL, ".UTF8") must be called. The
	// manifest makes argv[] use UTF-8 (which setlocale() cannot affect)
	// and the special setlocale() call makes mbrtowc() and such functions
	// use UTF-8. (It's weird why regular setlocale(LC_ALL, "") doesn't
	// use the code page from the application manifest.)
	//
	// The two things have quite a bit of overlap though. For example,
	// both affect the code page used in the file system APIs. Thus,
	// if argv[] isn't in UTF-8, using setlocale() to set UTF-8 will
	// break non-ASCII filenames that have been passed as command line
	// arguments. Thus, it's best to set an UTF-8 locale only when
	// the active code page is UTF-8.
	//
	// NOTE: Only UCRT supports the UTF-8 locale string, thus this
	// will fail with MSVCRT if the active code page is UTF-8. That
	// shouldn't be too bad because UTF-8 doesn't work properly with
	// MSVCRT anyway.
#	define tuklib_gettext_setlocale() \
		setlocale(LC_ALL, GetACP() == CP_UTF8 ? ".UTF8" : "")
#else
#	define tuklib_gettext_setlocale() setlocale(LC_ALL, "")
#endif

#ifndef TUKLIB_GETTEXT
#	ifdef ENABLE_NLS
#		define TUKLIB_GETTEXT 1
#	else
#		define TUKLIB_GETTEXT 0
#	endif
#endif

#if TUKLIB_GETTEXT
#	include <libintl.h>
#	define tuklib_gettext_init(package, localedir) \
		do { \
			tuklib_gettext_setlocale(); \
			bindtextdomain(package, localedir); \
			textdomain(package); \
		} while (0)
#	define _(msgid) gettext(msgid)
#else
#	define tuklib_gettext_init(package, localedir) \
		tuklib_gettext_setlocale()
#	define _(msgid) (msgid)
#	define ngettext(msgid1, msgid2, n) ((n) == 1 ? (msgid1) : (msgid2))
#endif
#define N_(msgid) msgid

#endif
