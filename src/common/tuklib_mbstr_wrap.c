// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_mbstr_wrap.c
/// \brief      Word wraps a string and prints it to a FILE stream
///
/// This depends on tuklib_mbstr_width.c.
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "tuklib_mbstr.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


extern int
tuklib_mbstr_wraps(FILE *outfile, const struct tuklib_mbstr_wrap *opt,
		const char *str)
{
	// left_cont may be less than left_margin. In that case, if the first
	// word is extremely long, it will stay on the first line even if
	// the line then gets overlong.
	//
	// On the other hand, tab_cont < tab isn't allowed because it
	// could result in inconsistent behavior when a very long word
	// comes right after a \t.
	//
	// It is fine to have tab < left_margin although it would be
	// an odd use case.
	if (!(opt->left_margin < opt->right_margin
			&& opt->left_cont < opt->right_margin
			&& opt->tab <= opt->tab_cont
			&& opt->tab_cont < opt->right_margin))
		return TUKLIB_MBSTR_WRAP_ERR_OPT;

	// This is set to TUKLIB_MBSTR_WRAP_WARN_OVERLONG if one or more
	// output lines extend past opt->right_margin columns.
	int warn_overlong = 0;

	// Indentation of the first output line after \n or \r.
	// \t sets this to opt->tab and \r back to the original value.
	size_t first_indent = opt->left_margin;

	// Indentation of the output lines that occur due to word wrapping.
	// \t sets this to opt->tab_cont and \r back to the original value.
	size_t cont_indent = opt->left_cont;

	// Spaces are printed only when there is something else to put
	// after the spaces on the line. This avoids unwanted empty lines
	// in the output and makes it possible to ignore possible spaces
	// before a \t character.
	size_t pending_spaces = first_indent;

	// Current output column. When cur_col == pending_spaces, nothing
	// has been actually printed to the current output line.
	size_t cur_col = pending_spaces;

	// True if the line-break opportunity at cur_col is a soft hyphen.
	bool lbo_shy = false;

	while (true) {
		// Number of bytes until the *next* line-break opportunity.
		size_t len = 0;

		// Number of columns until the *next* line-break opportunity.
		size_t width = 0;

		// This is 1 if the *next* line-break opportunity at
		// cur_col + width is a soft hyphen and 0 otherwise.
		//
		// Don't confuse this with lbo_shy which is about
		// the current line-break opportunity at cur_col.
		size_t shy_width = 0;

		while (true) {
			// Find the next character that we handle specially.
			const size_t n = strcspn(str + len, " \b\f\n\r\t\v");

			// Calculate how many columns the skipped
			// characters need.
			const size_t w = tuklib_mbstr_width_mem(str + len, n);
			if (w == (size_t)-1)
				return TUKLIB_MBSTR_WRAP_ERR_STR;

			width += w;
			len += n;

			// \b is used to encode a non-breaking space which
			// isn't a line-breaking opportunity. If \b is seen,
			// keep searching for the next special character.
			// \b cannot be omitted from the strcspn()
			// because wcwidth() (in tuklib_mbstr_width_mem())
			// wouldn't return 1 for \b.
			if (str[len] == '\b') {
				++width;
				++len;
				continue;
			}

			// This is a newline or a line-break opportunity.
			// If it is a soft hyphen, an extra column needs
			// to be reserved in case a hyphen is needed.
			shy_width = str[len] == '\f' ? 1 : 0;
			break;
		}

		// Determine if adding this chunk of text would make the
		// current output line exceed opt->right_margin columns.
		const bool too_long = cur_col + width + shy_width
				> opt->right_margin;

		// Wrap the line if needed.
		//
		// However, don't wrap if the current column is less than
		// where the continuation line would begin. In that case
		// the chunk wouldn't fit on the next line either so we
		// just have to produce an overlong line.
		//
		// Also, don't wrap if so far the line only contains spaces.
		// Wrapping in that case would leave an odd empty line.
		if (too_long && cur_col > cont_indent
				&& cur_col > pending_spaces) {
			// Print the hyphen if the line-break opportunity
			// at cur_col was due to a soft hyphen.
			if (lbo_shy && putc('-', outfile) == EOF)
				return TUKLIB_MBSTR_WRAP_ERR_IO;

			if (putc('\n', outfile) == EOF)
				return TUKLIB_MBSTR_WRAP_ERR_IO;

			// There might be trailing space, zero-width space,
			// soft hyphen, or newline characters which
			// need to be ignored to keep the output pretty.
			//
			// Spaces need to be ignored because in some
			// writing styles there are two spaces after
			// a full stop. Example string:
			//
			//     "Foo bar.  Abc def."
			//              ^
			// If the first space after the first full stop
			// triggers word wrapping, both spaces must be
			// ignored. Otherwise the next line would be
			// indented too much.
			//
			// Zero-width spaces and soft hyphens are ignored
			// the same way because these characters are
			// meaningless if an adjacent character is a space.
			//
			// After ignoring spaces, a newline character
			// (including the implicit newline at the end of
			// the input string) must be ignored too:
			//
			//     "Foo bar. \nAbc def."
			//              ^
			// If the trailing space after the first full stop
			// triggers word wrapping, the following newline must
			// be ignored to prevent an unwanted empty line
			// between the two lines "Foo bar." and "Abc def."
			//
			// Another example:
			//
			//     "Foo bar.  "
			//               ^
			// If the trailing spaces after the full stop trigger
			// word wrapping, the implicit newline at the end of
			// the string must be ignored to prevent an empty
			// line after the line "Foo bar.".
			while (*str == ' ' || *str == '\v' || *str == '\f')
				++str;

			if (*str == '\0')
				return warn_overlong;

			if (*str == '\n') {
				++str;
				pending_spaces = first_indent;

			} else if (*str == '\r') {
				// The newline is ignored but the resetting
				// of the indentation has to be done still.
				++str;
				first_indent = opt->left_margin;
				cont_indent = opt->left_cont;
				pending_spaces = first_indent;

			} else {
				// Continuation lines can be indented
				// differently than the first line.
				pending_spaces = cont_indent;
			}

			cur_col = pending_spaces;
			continue;
		}

		// Print the current chunk of text before the next
		// line-break opportunity. If the chunk was empty,
		// don't print anything so that the pending spaces
		// aren't printed on their own.
		if (len > 0) {
			while (pending_spaces > 0) {
				if (putc(' ', outfile) == EOF)
					return TUKLIB_MBSTR_WRAP_ERR_IO;

				--pending_spaces;
			}

			for (size_t i = 0; i < len; ++i) {
				// Soft hyphens (\f) in the middle of
				// a string are ignored.
				if (str[i] == '\f')
					continue;

				// Non-breaking spaces (\b) are printed
				// as spaces.
				const int c = str[i] == '\b' ? ' '
						: (unsigned char)str[i];
				if (putc(c, outfile) == EOF)
					return TUKLIB_MBSTR_WRAP_ERR_IO;
			}

			str += len;
			cur_col += width;

			// Remember if the line got overlong. If no other
			// errors occur, we return warn_overlong. It might
			// help in catching problematic strings.
			if (too_long)
				warn_overlong
					= TUKLIB_MBSTR_WRAP_WARN_OVERLONG;
		}

		// Handle the special character after the chunk of text.
		//
		// Remember that the next line-break opportunity isn't
		// a soft hyphen...
		lbo_shy = false;

		switch (*str) {
		case '\f':
			// ...except when it is a soft hyphen.
			lbo_shy = true;
			break;

		case ' ':
			// Regular space.
			++cur_col;
			++pending_spaces;
			break;

		case '\t':
			// Set the alternative indentation settings.
			first_indent = opt->tab;
			cont_indent = opt->tab_cont;

			// Start a new line if there is no room to add even
			// one space before reaching the column opt->tab.
			// Possible pending spaces before the \t are ignored.
			cur_col -= pending_spaces;
			if (first_indent > cur_col) {
				pending_spaces = first_indent - cur_col;
			} else {
				if (putc('\n', outfile) == EOF)
					return TUKLIB_MBSTR_WRAP_ERR_IO;

				pending_spaces = first_indent;
			}

			cur_col = first_indent;
			break;

		case '\n': // Newline without resetting indentation
		case '\r': // Newline that also resets the effect of \t
		case '\0': // Implicit newline at the end of the string
			if (putc('\n', outfile) == EOF)
				return TUKLIB_MBSTR_WRAP_ERR_IO;

			if (*str == '\0')
				return warn_overlong;

			if (*str == '\r') {
				first_indent = opt->left_margin;
				cont_indent = opt->left_cont;
			}

			pending_spaces = first_indent;
			cur_col = first_indent;
			break;
		}

		// Skip the specially-handled character.
		++str;
	}
}


extern int
tuklib_mbstr_wrapf(FILE *stream, const struct tuklib_mbstr_wrap *opt,
		const char *fmt, ...)
{
	va_list ap;
	char *buf;

#ifdef HAVE_VASPRINTF
	va_start(ap, fmt);
	const int n = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (n == -1)
		return TUKLIB_MBSTR_WRAP_ERR_FORMAT;
#else
	// Fixed buffer size is dumb but in practice one shouldn't need
	// huge strings for *formatted* output. This simple method is safe
	// with pre-C99 vsnprintf() implementations too which don't return
	// the required buffer size (they return -1 or buf_size - 1) or
	// which might not null-terminate the buffer in case it's too small.
	const size_t buf_size = 128 * 1024;
	buf = malloc(buf_size);
	if (buf == NULL)
		return TUKLIB_MBSTR_WRAP_ERR_FORMAT;

	va_start(ap, fmt);
	const int n = vsnprintf(buf, buf_size, fmt, ap);
	va_end(ap);

	if (n <= 0 || n >= (int)(buf_size - 1)) {
		free(buf);
		return TUKLIB_MBSTR_WRAP_ERR_FORMAT;
	}
#endif

	const int ret = tuklib_mbstr_wraps(stream, opt, buf);
	free(buf);
	return ret;
}
