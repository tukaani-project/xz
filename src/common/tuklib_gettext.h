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

#ifndef TUKLIB_GETTEXT
#	ifdef ENABLE_NLS
#		define TUKLIB_GETTEXT 1
#	else
#		define TUKLIB_GETTEXT 0
#	endif
#endif

#if TUKLIB_GETTEXT
#	if defined(_WIN32) && !defined(__CYGWIN__)
		// <libintl.h> from gettext-runtime redirects setlocale()
		// to libintl_setlocale(). As of gettext 0.22.5 (and probably
		// 0.23), libintl_setlocale(LC_ALL, "") doesn't set the locale
		// to UTF-8 if UTF-8 code page has been set in the application
		// manifest. For example, one may get "fi_FI" when native
		// setlocale() would result in "Finnish_Finland.utf8". The
		// lack of ".utf8" (or equivalent) suffix results in garbled
		// non-ASCII chars in translatated messages and also affects
		// functions like mbrtowc() which depend on LC_CTYPE.
		//
		// Workaround the problem by not using libintl_setlocale()
		// for now. Notes:
		//
		// (1) libintl_setlocale() reads LC_* environment variables
		//     but native setlocale() doesn't. The loss of this
		//     feature doesn't matter too much because, on Windows,
		//     libintl still reads the env vars LANGUAGE, LC_ALL,
		//     LC_MESSAGES, and LANG when translating messages in
		//     the LC_MESSAGES category (other categories are very
		//     rarely used for translations). As of Gettext commit
		//     e18edc579 and Gnulib commit 9e301775ff:
		//
		//     libintl_gettext()
		//      `-- libintl_dcgettext()
		//           `-- libintl_dcigettext()
		//                `-- guess_category_value()
		//                     |-- gl_locale_name_posix()
		//                     |    `-- gl_locale_name_posix_unsafe()
		//                     |         `-- gl_locale_name_environ()
		//                     |              |-- getenv("LC_ALL")
		//                     |              |-- getenv("LC_MESSAGES")
		//                     |              `-- getenv("LANG")
		//                     `-- getenv("LANGUAGE")
		//
		// (2) If locale is changed, libintl_setlocale() marks cached
		//     translations as invalid. bindtextdomain(), which we
		//     call immediately after setlocale(), does the same
		//     invalidation too. Thus it doesn't matter in the
		//     tuklib_gettext_init() macro. It could matter if the
		//     application calls setlocale() elsewhere though (but
		//     then it's not guaranteed that such code even includes
		//     <libint.h> in addition to <locale.h>).
		//
		// This macro is checked by <libintl.h> since Gettext 0.18.2
		// (2012-12-08). When this is defined, setlocale() isn't
		// overridden.
		//
		// FIXME: Remove this hack when it's no longer needed.
#		define GNULIB_defined_setlocale 1
#	endif
#	include <libintl.h>
#	define tuklib_gettext_init(package, localedir) \
		do { \
			setlocale(LC_ALL, ""); \
			bindtextdomain(package, localedir); \
			textdomain(package); \
		} while (0)
#	define _(msgid) gettext(msgid)
#else
#	define tuklib_gettext_init(package, localedir) \
		setlocale(LC_ALL, "")
#	define _(msgid) (msgid)
#	define ngettext(msgid1, msgid2, n) ((n) == 1 ? (msgid1) : (msgid2))
#endif
#define N_(msgid) msgid

#endif
