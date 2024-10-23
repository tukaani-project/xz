// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_mbstr.h
/// \brief      Utility functions for handling multibyte strings
///
/// If not enough multibyte string support is available in the C library,
/// these functions keep working with the assumption that all strings
/// are in a single-byte character set without combining characters, e.g.
/// US-ASCII or ISO-8859-*.
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TUKLIB_MBSTR_H
#define TUKLIB_MBSTR_H

#include "tuklib_common.h"
#include <stdio.h>

TUKLIB_DECLS_BEGIN

#define tuklib_mbstr_width TUKLIB_SYMBOL(tuklib_mbstr_width)
extern size_t tuklib_mbstr_width(const char *str, size_t *bytes);
///<
/// \brief      Get the number of columns needed for the multibyte string
///
/// This is somewhat similar to wcswidth() but works on multibyte strings.
///
/// \param      str         String whose width is to be calculated.
/// \param      bytes       If this is not NULL, *bytes is set to the
///                         value returned by strlen(str) (even if an
///                         error occurs when calculating the width).
///
/// \return     On success, the number of columns needed to display the
///             string e.g. in a terminal emulator is returned. On error,
///             (size_t)-1 is returned. Possible errors include invalid,
///             partial, or non-printable multibyte character in str.

#define tuklib_mbstr_width_mem TUKLIB_SYMBOL(tuklib_mbstr_width_mem)
extern size_t tuklib_mbstr_width_mem(const char *str, size_t len);
///<
/// \brief      Get the number of columns needed for the multibyte buffer
///
/// This is like tuklib_mbstr_width() except that this takes the buffer
/// length in bytes as the second argument. This allows using the function
/// for buffers that aren't terminated with '\0'.
///
/// \param      str         String whose width is to be calculated.
/// \param      len         Number of bytes to read from str.
///
/// \return     On success, the number of columns needed to display the
///             string e.g. in a terminal emulator is returned. On error,
///             (size_t)-1 is returned. Possible errors include invalid,
///             partial, or non-printable multibyte character in str.

#define tuklib_mbstr_fw TUKLIB_SYMBOL(tuklib_mbstr_fw)
extern int tuklib_mbstr_fw(const char *str, int columns_min);
///<
/// \brief      Get the field width for printf() e.g. to align table columns
///
/// Printing simple tables to a terminal can be done using the field field
/// feature in the printf() format string, but it works only with single-byte
/// character sets. To do the same with multibyte strings, tuklib_mbstr_fw()
/// can be used to calculate appropriate field width.
///
/// The behavior of this function is undefined, if
///   - str is NULL or not terminated with '\0';
///   - columns_min <= 0; or
///   - the calculated field width exceeds INT_MAX.
///
/// \return     If tuklib_mbstr_width(str, NULL) fails, -1 is returned.
///             If str needs more columns than columns_min, zero is returned.
///             Otherwise a positive integer is returned, which can be
///             used as the field width, e.g. printf("%*s", fw, str).


/// One or more output lines exceeded right_margin.
/// This only a warning; everything was still printed successfully.
#define TUKLIB_MBSTR_WRAP_WARN_OVERLONG 1

/// Error writing to to the output FILE.
#define TUKLIB_MBSTR_WRAP_ERR_IO        2

/// Invalid options in struct tuklib_mbstr_wrap.
#define TUKLIB_MBSTR_WRAP_ERR_OPT       4

/// Invalid or unsupported multibyte character in the input string:
/// either mbrtowc() failed or wcwidth() returned a negative value.
#define TUKLIB_MBSTR_WRAP_ERR_STR       8

/// Only tuklib_mbstr_wrapf(): Error in converting the format string.
/// It's either a memory allocation failure or something bad with the
/// format string or arguments.
#define TUKLIB_MBSTR_WRAP_ERR_FORMAT   16

/// Options for tuklib_mbstr_wrap{s,f} functions
struct tuklib_mbstr_wrap {
	/// Indentation of the first output line after `\n` or `\r`.
	/// This can be anything less than right_margin.
	unsigned short left_margin;

	/// Column where word-wrapped continuation lines start.
	/// This can be anything less than right_margin.
	unsigned short left_cont;

	/// Column where the text after `\t` will start, either on the current
	/// line (when there is room to add at least one space) or on a new
	/// empty line.
	unsigned short tab;

	/// Like left_cont but for text after a `\t`. However, this must
	/// be greater than or equal to tab.
	unsigned short tab_cont;

	/// For 80-column terminals, it is recommended to use 79 here for
	/// maximum portability. 80 will work most of the time but it will
	/// result in unwanted empty lines in the rare case where a terminal
	/// moves the cursor to the beginning of the next line immediately
	/// when the last column has been used.
	unsigned short right_margin;
};

#define tuklib_mbstr_wraps TUKLIB_SYMBOL(tuklib_mbstr_wraps)
extern int tuklib_mbstr_wraps(FILE *stream,
		const struct tuklib_mbstr_wrap *opt, const char *str);
///<
/// \brief      Word-wrap a multibyte string and write it to a FILE
///
/// This is intended to be usable, for example, for printing
/// (translated) --help text in command line tools.
///
/// Word wrapping is done only at spaces and at the special control characters
/// described below. Multiple consecutive spaces are handled properly: strings
/// that have two (or more) spaces after a full sentence will look good even
/// when the spaces occur at a word-wrapping boundary. Trailing spaces are
/// ignored at the end of a line or at the end of a string.
///
/// The following control chararacters have been repurposed:
///
///   - `\b` = Non-breaking space is printed as a regular space but it isn't
///            a line-break opportunity.
///   - `\v` = Zero-width space allows a line break without producing any
///            output by itself. This can be useful after hard hyphens as
///            hyphens aren't otherwise used for line breaking. This can
///            also be useful in languages that don't use spaces between words.
///   - `\f` = Soft hyphen is like zero-width space except it becomes as hyphen
///            if the line is wrapped at that position.
///   - `\t` = Change to alternative indentation (left margins).
///   - `\r` = Reset back to the initial indentation and add a newline.
///   - `\n` = Add a newline without resetting the effect of `\t`.
///
/// \param      stream      Output FILE stream. For decent performance, it
///                         should be in buffered mode because this function
///                         writes the output one byte at a time with fputc().
/// \param      opt         Word wrapping options.
/// \param      str         Null-terminated multibyte string that is in
///                         the encoding used by the current locale.
///
/// \return     Returns 0 on success. If an error or warning occurs, one of
///             TUKLIB_MBSTR_WRAP_* codes is returned. Those codes are powers
///             of two. When warning/error detection can be delayed, the
///             return values can be accumulated from multiple calls using
///             bitwise-or into a single variable which can be checked after
///             all strings have (hopefully) been printed.

#define tuklib_mbstr_wrapf TUKLIB_SYMBOL(tuklib_mbstr_wrapf)
extern int tuklib_mbstr_wrapf(FILE *stream,
		const struct tuklib_mbstr_wrap *opt, const char *fmt, ...);
///<
/// \brief      Format and word-wrap a multibyte string and write it to a FILE
///
/// This is like tuklib_mbstr_wraps() except that this takes a printf()
/// format string.
///
/// \note       On platforms that lack vasprintf(), the intermediate
///             result from vsnprintf() must fit into a 128 KiB buffer.
///             TUKLIB_MBSTR_WRAP_ERR_FORMAT is returned if it doesn't
///             but only on platforms that lack vasprintf().

TUKLIB_DECLS_END
#endif
