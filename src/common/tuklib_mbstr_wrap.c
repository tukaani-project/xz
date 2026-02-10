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
#include "tuklib_mbstr_wrap.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Allow disabling the RTL code by defining it to 0.
#ifndef TUKLIB_MBSTR_WRAP_ENABLE_RTL
#	define TUKLIB_MBSTR_WRAP_ENABLE_RTL 1
#endif

#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
// A few BiDi control characters (unneeded are commented out):
#define LRM "\342\200\216" /* U+200E as UTF-8, left-to-right mark */
#define RLM "\342\200\217" /* U+200F as UTF-8, right-to-left mark */

// #define LRE "\342\200\252" /* U+202A as UTF-8, left-to-right embedding */
#define RLE "\342\200\253" /* U+202B as UTF-8, right-to-left embedding */

#define LRI "\342\201\246" /* U+2066 as UTF-8, left-to-right isolate */
#define RLI "\342\201\247" /* U+2067 as UTF-8, right-to-left isolate */
// #define FSI "\342\201\250" /* U+2068 as UTF-8, first strong isolate */
#define PDI "\342\201\251" /* U+2069 as UTF-8, pop directional isolate */
#endif


extern int
tuklib_wraps(FILE *outfile, const struct tuklib_wrap_opt *opt, const char *str)
{
	// left_cont may be less than left_margin. In that case, if the first
	// word is extremely long, it will stay on the first line even if
	// the line then gets overlong.
	//
	// On the other hand, left2_cont < left2_margin isn't allowed because
	// it could result in inconsistent behavior when a very long word
	// comes right after a \v.
	//
	// It is fine to have left2_margin < left_margin although it would be
	// an odd use case.
	if (!(opt->left_margin < opt->right_margin
			&& opt->left_cont < opt->right_margin
			&& opt->left2_margin <= opt->left2_cont
			&& opt->left2_cont < opt->right_margin
			&& (opt->flags & ~3) == 0))
		return TUKLIB_WRAP_ERR_OPT;

	// This is set to TUKLIB_WRAP_WARN_OVERLONG if one or more
	// output lines extend past opt->right_margin columns.
	int warn_overlong = 0;

	// Indentation of the first output line after \n or \r.
	// \v sets this to opt->left2_margin.
	// \r resets this back to the original value.
	size_t first_indent = opt->left_margin;

	// Indentation of the output lines that occur due to word wrapping.
	// \v sets this to opt->left2_cont and \r back to the original value.
	size_t cont_indent = opt->left_cont;

	// If word wrapping occurs, the newline isn't printed unless more
	// text would be put on the continuation line. This is also used
	// when \v needs to start on a new line.
	bool pending_newline = false;

	// Spaces are printed only when there is something else to put
	// after the spaces on the line. This avoids unwanted empty lines
	// in the output and makes it possible to ignore possible spaces
	// before a \v character.
	size_t pending_spaces = first_indent;

	// Current output column. When cur_col == pending_spaces, nothing
	// has been actually printed to the current output line (unless
	// the active left margin is 0, and the input line begins with
	// zero-width character(s) followed by \t).
	size_t cur_col = pending_spaces;

#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
	// The following variables only matter in RTL mode.
	//
	// (It is easier to understand this function if one first reads
	// the code while pretending that these RTL variables and the
	// related code don't exist. Then read the note at the end of
	// this file, and finally read the RTL sections of the code.)
	//
	// When starting a new output line, the BiDi control characters
	// LRI/RLI/RLE must be placed in the correct output column to
	// ensure that the text aligns correctly in all cases. The column is
	// either first_indent or cont_indent (and those depend on \v and \r).
	size_t bidi_ctrl_col = first_indent;

	// When true, an isolate initiator has been written out for the
	// text that comes before \v. When the \v is reached, the isolate
	// will be closed and this will be set back to false. This will also
	// be set to false if wrapping occurs before the \v is reached
	// because a newline implicitly terminates the isolate.
	bool in_pre_v_isolate = false;
#endif

	while (true) {
		// Number of bytes until the *next* line-break opportunity.
		size_t len = 0;

		// Number of columns until the *next* line-break opportunity.
		size_t width = 0;

		// Text between a pair of \b characters is treated as
		// an unbreakable block even if it contains spaces.
		// It must not contain any control characters before
		// the closing \b.
		bool unbreakable = false;

		while (true) {
			// Find the next character that we handle specially.
			// In an unbreakable block, search only for the
			// closing \b; if missing, the unbreakable block
			// extends to the end of the string.
			const size_t n = strcspn(str + len,
					unbreakable ? "\b" : " \t\n\r\v\b");

			// Calculate how many columns the characters need.
			const size_t w = tuklib_mbstr_width_mem(str + len, n);
			if (w == (size_t)-1)
				return TUKLIB_WRAP_ERR_STR;

			width += w;
			len += n;

			// \b isn't a line-break opportunity so it has to
			// be handled here. For simplicity, empty blocks
			// are treated as zero-width characters.
			if (str[len] == '\b') {
				++len;
				unbreakable = !unbreakable;
				continue;
			}

			break;
		}

		// Determine if adding this chunk of text would make the
		// current output line exceed opt->right_margin columns.
		const bool too_long = cur_col + width > opt->right_margin;

		// Wrap the line if needed. However:
		//
		//   - Don't wrap if the current column is less than where
		//     the continuation line would begin. In that case
		//     the chunk wouldn't fit on the next line either so
		//     we just have to produce an overlong line.
		//
		//   - Don't wrap if so far the line only contains spaces.
		//     Wrapping in that case would leave a weird empty line.
		//     NOTE: This "only contains spaces" condition is the
		//     reason why left2_margin > left2_cont isn't allowed.
		if (too_long && cur_col > cont_indent
				&& cur_col > pending_spaces) {
			// There might be trailing spaces or zero-width spaces
			// which need to be ignored to keep the output pretty.
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
			// Zero-width spaces are ignored the same way
			// because they are meaningless if an adjacent
			// character is a space.
			while (*str == ' ' || *str == '\t')
				++str;

			// Don't print the newline here; only mark it as
			// pending. This avoids an unwanted empty line if
			// there is a \n or \r or \0 after the spaces have
			// been ignored.
			pending_newline = true;
			pending_spaces = cont_indent;
			cur_col = pending_spaces;
#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			bidi_ctrl_col = pending_spaces;
			in_pre_v_isolate = false;
#endif

			// Since str may have been incremented due to the
			// ignored spaces, the loop needs to be restarted.
			continue;
		}

		// Print the current chunk of text before the next
		// line-break opportunity. If the chunk was empty,
		// don't print anything so that the pending newline
		// and pending spaces aren't printed on their own.
		if (len > 0) {
			if (pending_newline) {
				pending_newline = false;
				if (putc('\n', outfile) == EOF)
					return TUKLIB_WRAP_ERR_IO;
			}

#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			// In RTL mode: True if RLE needs to be added *after*
			// the current chunk of text.
			bool delayed_rle = false;

			if ((opt->flags & TUKLIB_WRAP_F_RTL)
					&& cur_col == pending_spaces) {
				// We are in RTL mode and haven't printed
				// anything on this output line yet.
				const char *ctrl = RLM RLI;

				// pending_spaces > bidi_ctrl_col is possible
				// if the input line begins with spaces.
				size_t target_col = bidi_ctrl_col;
				pending_spaces -= bidi_ctrl_col;

				// See if this input line contains a \v.
				const size_t n = strcspn(str, "\n\r\v");
				if (str[n] == '\v') {
					// We hope that opt->left* are all
					// greater than zero. That is, when
					// \v is used, we don't attempt
					// to avoid putting BiDi control chars
					// in the first column.
					ctrl = (opt->flags
						& TUKLIB_WRAP_F_RTL_BOTH)
							? RLM RLI : RLM LRI;
					in_pre_v_isolate = true;
				} else if (cur_col == 0) {
					// Try to workaround the issue that
					// some terminals ignore BiDi control
					// chars in the first column. If the
					// input line begins with a strong
					// RTL character, it can set the
					// base direction for the line.
					//
					// The lowest RTL code point is U+0590.
					// Exclude U+2000 to U+2FFF because
					// that range has BiDi control chars
					// (which would be a problem here)
					// and no Bidi_Class L or AL chars.
					//
					// These UTF-8 prefix checks are crude
					// but they should be enough when the
					// text contains RTL words mixed with
					// a few US-ASCII LTR strings.
					//
					// Check also the width of the current
					// chunk of text. If it only contains
					// zero-width chars, then we have to
					// add a space anyway.
					const int c = (unsigned char)str[0];
					if (c >= 0xD4 && c != 0xE2
							&& width > 0) {
						// Assume/guess/hope that
						// the first word is RTL.
						ctrl = "";
						delayed_rle = true;
					} else {
						// The first char isn't RTL.
						// Add a space to make line
						// direction autodetection
						// work in terminals that
						// ignore BiDi control chars
						// in the first column.
						++target_col;
						++cur_col;
					}
				}

				for (size_t i = 0; i < target_col; ++i)
					if (putc(' ', outfile) == EOF)
						return TUKLIB_WRAP_ERR_IO;

				if (fputs(ctrl, outfile) == EOF)
					return TUKLIB_WRAP_ERR_IO;
			}
#endif

			while (pending_spaces > 0) {
				if (putc(' ', outfile) == EOF)
					return TUKLIB_WRAP_ERR_IO;

				--pending_spaces;
			}

			for (size_t i = 0; i < len; ++i) {
				// Ignore unbreakable block characters (\b).
				const int c = (unsigned char)str[i];
				if (c != '\b' && putc(c, outfile) == EOF)
					return TUKLIB_WRAP_ERR_IO;
			}

#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			// In RTL mode: Add delayed RLE if needed.
			if (delayed_rle && fputs(RLE, outfile) == EOF)
				return TUKLIB_WRAP_ERR_IO;
#endif

			str += len;
			cur_col += width;

			// Remember if the line got overlong. If no other
			// errors occur, we return warn_overlong. It might
			// help in catching problematic strings.
			if (too_long)
				warn_overlong = TUKLIB_WRAP_WARN_OVERLONG;
		}

		// Handle the special character after the chunk of text.
		switch (*str) {
		case ' ':
			// Regular space.
			++cur_col;
			++pending_spaces;
			break;

		case '\v':
			// Set the alternative indentation settings.
			first_indent = opt->left2_margin;
			cont_indent = opt->left2_cont;

#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			if (in_pre_v_isolate) {
				in_pre_v_isolate = false;

				// Pad the text before \v using spaces up to
				// opt->left_margin2 - 1 (first_indent - 1).
				// When multiple lines with \v are printed
				// with the same options, each isolate will
				// have the same width.[*] This makes
				// columns align neatly in all cases
				// (LTR or RTL content before \v, and
				// terminal in the LTR or RTL base direction).
				//
				// [*] Except those that are too wide to fit
				//     within opt->left2_margin or which are
				//     so wide that they word wrap to more
				//     than one line (word wrapping doesn't
				//     do this padding).
				if (first_indent > cur_col) {
					pending_spaces += first_indent - 1
							- cur_col;
					cur_col = first_indent - 1;
				}

				// Don't add LRM/RLM unless at least one
				// space needs to be printed. This works
				// around a problem in VTE-based terminal
				// emulators where RLI "(123)" RLM PDI may
				// be displayed as ")123)". The string is
				// displayed correctly if there is at least
				// one space before RLM.
				//
				// The LRM/RLM is needed so that the padding
				// remains effective if there won't be any
				// non-whitespace non-formatting characters
				// after the padding.
				if (pending_spaces > 0) {
					do {
						if (putc(' ', outfile) == EOF)
							return TUKLIB_WRAP_ERR_IO;
					} while (--pending_spaces > 0);

					const char *ctrl = (opt->flags
						& TUKLIB_WRAP_F_RTL_BOTH)
							? RLM : LRM;
					if (fputs(ctrl, outfile) == EOF)
						return TUKLIB_WRAP_ERR_IO;
				}

				if (fputs(PDI, outfile) == EOF)
					return TUKLIB_WRAP_ERR_IO;

				// If the text after \v might fit on this
				// output line, add a space and initiate
				// a new isolate (always RTL).
				if (first_indent > cur_col) {
					if (fputs(" " RLI, outfile) == EOF)
						return TUKLIB_WRAP_ERR_IO;

					// We have printed all the necessary
					// spaces. Set pending_spaces so that
					// it will become zero in
					// "if (first_indent > cur_col)" below.
					pending_spaces = (size_t)-1;
				}
			}
#endif

			if (first_indent > cur_col) {
				// Add one or more spaces to reach
				// the column specified in first_indent.
				pending_spaces += first_indent - cur_col;
			} else {
				// There is no room to add even one space
				// before reaching the column first_indent.
				pending_newline = true;
				pending_spaces = first_indent;
			}

			cur_col = first_indent;
#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			bidi_ctrl_col = first_indent;
#endif
			break;

		case '\0': // Implicit newline at the end of the string.
		case '\r': // Newline that also resets the effect of \v.
		case '\n': // Newline without resetting the indentation mode.
			if (putc('\n', outfile) == EOF)
				return TUKLIB_WRAP_ERR_IO;

			if (*str == '\0')
				return warn_overlong;

			if (*str == '\r') {
				first_indent = opt->left_margin;
				cont_indent = opt->left_cont;
			}

			pending_newline = false;
			pending_spaces = first_indent;
			cur_col = first_indent;
#if TUKLIB_MBSTR_WRAP_ENABLE_RTL
			bidi_ctrl_col = first_indent;
#endif
			break;
		}

		// Skip the specially-handled character.
		++str;
	}
}


extern int
tuklib_wrapf(FILE *stream, const struct tuklib_wrap_opt *opt,
		const char *fmt, ...)
{
	va_list ap;
	char *buf;

#ifdef HAVE_VASPRINTF
	va_start(ap, fmt);

#ifdef __clang__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	const int n = vasprintf(&buf, fmt, ap);
#ifdef __clang__
#	pragma GCC diagnostic pop
#endif

	va_end(ap);
	if (n == -1)
		return TUKLIB_WRAP_ERR_FORMAT;
#else
	// Fixed buffer size is dumb but in practice one shouldn't need
	// huge strings for *formatted* output. This simple method is safe
	// with pre-C99 vsnprintf() implementations too which don't return
	// the required buffer size (they return -1 or buf_size - 1) or
	// which might not null-terminate the buffer in case it's too small.
	const size_t buf_size = 128 * 1024;
	buf = malloc(buf_size);
	if (buf == NULL)
		return TUKLIB_WRAP_ERR_FORMAT;

	va_start(ap, fmt);
	const int n = vsnprintf(buf, buf_size, fmt, ap);
	va_end(ap);

	if (n <= 0 || n >= (int)(buf_size - 1)) {
		free(buf);
		return TUKLIB_WRAP_ERR_FORMAT;
	}
#endif

	const int ret = tuklib_wraps(stream, opt, buf);
	free(buf);
	return ret;
}


/*

The right-to-left (RTL) mode
============================

Terminology:

  - An input string can contain one or more input lines.

  - Each input line is terminated with \n, \r, or \0.

  - One input line may become multiple output lines due to wrapping.

  - "Paragraph direction" and "base direction" mean the same thing, which
    in terminal emulator context is an input line that ends with a \n.
    A grammatical paragraph of text in a hardwrapped plain text file thus
    contains multiple "paragraphs" in terminal emulator context.

  - Some terminals autodetect the base direction from the first strong
    character on the output line. RLM is an invisible strong character.
    Lines with RTL base direction are aligned to the right edge of the
    terminal. If autodetection is disabled or not supported, all lines
    typically have LTR base direction and are aligned to the left edge.


Working with a forced LTR base direction
----------------------------------------

If a terminal supports BiD text but forces the base direction to LTR,
it looks a bit odd for RTL language, just like it would if English text
was right-justified. When a RTL text contains a word or two in LTR,
then the result is a mess if the base direction is LTR.

Consider a sentence "USE cp TO COPY FILES AND mv TO MOVE THEM." where
UPPER CASE is RTL and lower case is LTR. If it was rendered using RTL
base direction, it would look like this (the reading directions are
marked below):

    .MEHT EVOM OT mv DNA SELIF YPOC OT cp ESU
                                          <<<
                                       >>
                     <<<<<<<<<<<<<<<<<
                  >>
    <<<<<<<<<<<<<

That is, the LTR words are displayed LTR and the rest RTL. The sentence
starts at the right-hand side and progresses to the left; only the two
short LTR pieces interrupt it. But with the LTR base direction, the
sentence starts and ends at the middle of the line, except the full stop
is at the right-hand side:

    ESU cp DNA SELIF YPOC OT mv MEHT EVOM OT.
    <<<
        >>
           <<<<<<<<<<<<<<<<<
                             >>
                                <<<<<<<<<<<<
                                            >

Reversing it to LTR shows how terrible it looks:

    .TO MOVE THEM mv TO COPY FILES AND cp USE
                                          >>>
                                       >>
                     >>>>>>>>>>>>>>>>>
                  >>
     >>>>>>>>>>>>
    >

When the base direction is LTR, the word order can be fixed by wrapping
the RTL text in RLE...PDF or RLI...PDI. The forced LTR base direction
will still align the text to the left.


Implementation idea
-------------------

Ideally we would do this:

    RLM SPACES RLI ... PDI                         Lines without \v
    RLM SPACES LRI ... LRM PDI SPACE RLI ... PDI   Lines with \v (LTR \v RTL)
    RLM SPACES RLI ... RLM PDI SPACE RLI ... PDI   Lines with \v (RTL \v RTL)

Notes:

  - RLM sets the base direction to RTL in terminals that use autodetection.

  - SPACES indent to the requested margin.

  - All text is wrapped in isolates. If the RTL text contains an LTR word,
    the RTL text will still have correct word order even if the terminal
    forces LTR base direction.

  - If \v is used and the text in the the first isolate is short, the
    isolate is padded by adding spaces at the end. If the base direction
    of the line and the direction of the padded isolate are different,
    the padding spaces push the text away from the terminal's edge.
    (Think of "LTR text   " that is right-aligned in a RTL terminal.)

    The padding is terminated using LRM or RLM (matching the direction
    of the isolate) before PDI. This is required in case the output
    line won't have any other text than whitespace and isolate formatting
    characters. Consider an example without LRM/RLM before PDI:

        RLM SPACES RLI "--option    " PDI SPACE RLI

    After the character "n" there are only spaces, PDI, and RLI. In
    this situation, the padding after the "n" isn't effective because
    the BiDi algorithm moves all trailing whitespace and isolate
    formatting characters to the visual end of the line.[*] The problem
    is fixed by adding a zero-width non-whitespace character after the
    padding. A zero-width space wouldn't work but LRM and RLM do.

    [*] UAX #9: Unicode Bidirectional Algorithm, section 3.4, rule L1
        https://www.unicode.org/reports/tr9/#L1

The final PDI isn't mandatory because the last isolate is terminated at
a newline anyway, so the above can be slightly simplified to this:

    RLM SPACES RLI ...                             Lines without \v
    RLM SPACES LRI ... LRM PDI SPACE RLI ...       Lines with \v (LTR \v RTL)
    RLM SPACES RLI ... RLM PDI SPACE RLI ...       Lines with \v (RTL \v RTL)

As of 2026, the above works in a text editor, but it doesn't work in some
terminal emulators.


Workaround 1: The first column
------------------------------

Konsole and VTE-based terminals ignore all BiDi control characters in
the first column (including LRI/LRM, LRI/RLI, and LRE/RLE). In VTE this
is a known hard-to-fix issue. This can be worked around by moving the
RLM after the indentation spaces:

    SPACES RLM RLI ...                             Lines without \v
    SPACES RLM LRI ... LRM PDI SPACE RLI ...       Lines with \v (LTR \v RTL)
    SPACES RLM RLI ... RLM PDI SPACE RLI ...       Lines with \v (RTL \v RTL)

But this only works if the margin at the beginning of the line isn't zero.
In this case, a more hacky workaround is used:

If an input line doesn't contain \v and the left margin for the current
output line is zero, extremely simple heuristics are used to guess if
the first character might be a strong RTL (Bidi_Class R or AL). If it
isn't, one space is added at the beginning of the line and then the
format described above is used.

Adding a space looks ugly, so if the first character is guessed to be
strong RTL, we let it set the base direction for terminals that autodetect
it. For terminals that force LTR base direction, we add RLE at the first
line-break opportunity:

    "Hopefully-RTL-word" RLE " and the rest of RTL text."

Note that RLI wouldn't work here; we must use RLE.

If a line contains \v and the left margin for the current output line
is zero, then no workaround is applied and the output will be messed up
in terminals that ignore BiDi control characters in the first column.
Thus, one should always set left_margin, left_cont, left2_margin, and
left2_cont to non-zero values when using \v!


Workaround 2: Avoid unnecessary LRM/RLM before PDI
--------------------------------------------------

In VTE-based terminals, RLM after a closing parenthesis may affect
the mirroring. The '|' char depicts the right edge of the terminal:

    $ printf '\e[?2501h'  # Enable autodetection.
    $ printf ' \u200f\u2067 (1234)\u200f\u2069 789\n'
                                                          789 )1234)  |
    $ printf ' \u200f\u2067 (1234) \u200f\u2069 789\n'
                                                         789  (1234)  |

Thus, add RLM before PDI only if the previous character is a space.

The problem doesn't seem to occur with PDI LRM. Applying the workaround
also in this case makes the code simpler though.


Known issues
------------

(1)
Some terminals don't support isolates properly even if they have some
BiDi support. For example:

  - Isolate formatting characters might display as spaces or boxes
    instead of being invisible.

  - Isolates might be treated like embeddings (LRE/RLE...PDF).

(2)
When \v i used, an unneeded newline may appear if the text before \v
is LTR and has leading spaces. Leading spaces are common when command
line options are listed like "-o, --option" and some only have a long
option. In that case, the "-o," part is replaced with spaces to keep
the long options aligned. In RTL mode, these leading LTR spaces can
trigger an unneeded newline, which are depicted with "%%%" below:

    |                      <some_RTL_text>  <padded_LTR_text____>  |
    |                  this is some option  -s, --some-option      |
    |                          foo and bar  -f, --foobar           |
    |                                    %%% --a-very-long-option  |
    |           it is a fairly long option                         |

That is, "--a-very-long-option" isn't wide enough that the description
wouldn't fit on the same line, but the leading spaces don't fit. While
it's not great, it likely won't be fixed.

*/
