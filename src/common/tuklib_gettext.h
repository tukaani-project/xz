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

// Optional: Strings that are word wrapped using tuklib_mbstr_wrap may be
// marked with W_("foo) in the source code. xgettext can then add a comment
// to all such strings to inform translators if the following option is added
// to XGETTEXT_OPTIONS in po/Makevars or in an equivalent place:
//
// '--keyword=W_:1,"Automatically word-wrapped at spaces. Supports extra markup: \v = zero-width space; \f = soft hyphen; \b = non-breaking space"'
//
// NOTE: The double-quotes in the --keyword argument above must be passed to
// xgettext as is, thus the whole argument may need to be in single-quotes.
#define W_(msgid) _(msgid)

#endif
