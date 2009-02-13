///////////////////////////////////////////////////////////////////////////////
//
/// \file       message.c
/// \brief      Printing messages to stderr
//
//  Copyright (C) 2007-2008 Lasse Collin
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

#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#ifdef _WIN32
#	ifndef _WIN32_WINNT
#		define _WIN32_WINNT 0x0500
#	endif
#	include <windows.h>
#endif

#include <stdarg.h>


/// Name of the program which is prefixed to the error messages.
static const char *argv0;

/// Number of the current file
static unsigned int files_pos = 0;

/// Total number of input files; zero if unknown.
static unsigned int files_total;

/// Verbosity level
static enum message_verbosity verbosity = V_WARNING;

/// Filename which we will print with the verbose messages
static const char *filename;

/// True once the a filename has been printed to stderr as part of progress
/// message. If automatic progress updating isn't enabled, this becomes true
/// after the first progress message has been printed due to user sending
/// SIGALRM. Once this variable is true,  we will print an empty line before
/// the next filename to make the output more readable.
static bool first_filename_printed = false;

/// This is set to true when we have printed the current filename to stderr
/// as part of a progress message. This variable is useful only if not
/// updating progress automatically: if user sends many SIGALRM signals,
/// we won't print the name of the same file multiple times.
static bool current_filename_printed = false;

/// True if we should print progress indicator and update it automatically.
static bool progress_automatic;

/// This is true when a progress message was printed and the cursor is still
/// on the same line with the progress message. In that case, a newline has
/// to be printed before any error messages.
static bool progress_active = false;

/// Expected size of the input stream is needed to show completion percentage
/// and estimate remaining time.
static uint64_t expected_in_size;

/// Time when we started processing the file
static double start_time;

/// The signal handler for SIGALRM sets this to true. It is set back to false
/// once the progress message has been updated.
static volatile sig_atomic_t progress_needs_updating = false;


#ifdef _WIN32

static HANDLE timer_queue = NULL;
static HANDLE timer_timer = NULL;


static void CALLBACK
timer_callback(PVOID dummy1 lzma_attribute((unused)),
		BOOLEAN dummy2 lzma_attribute((unused)))
{
	progress_needs_updating = true;
	return;
}


/// Emulate alarm() on Windows.
static void
my_alarm(unsigned int seconds)
{
	// Just in case creating the queue has failed.
	if (timer_queue == NULL)
		return;

	// If an old timer_timer exists, get rid of it first.
	if (timer_timer != NULL) {
		(void)DeleteTimerQueueTimer(timer_queue, timer_timer, NULL);
		timer_timer = NULL;
	}

	// If it fails, tough luck. It's not that important.
	(void)CreateTimerQueueTimer(&timer_timer, timer_queue, &timer_callback,
			NULL, 1000U * seconds, 0,
			WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE);

	return;
}

#else

#define my_alarm alarm

/// Signal handler for SIGALRM
static void
progress_signal_handler(int sig lzma_attribute((unused)))
{
	progress_needs_updating = true;
	return;
}

#endif

/// Get the current time as double
static double
my_time(void)
{
	struct timeval tv;

	// This really shouldn't fail. I'm not sure what to return if it
	// still fails. It doesn't look so useful to check the return value
	// everywhere. FIXME?
	if (gettimeofday(&tv, NULL))
		return -1.0;

	return (double)(tv.tv_sec) + (double)(tv.tv_usec) / 1.0e9;
}


/// Wrapper for snprintf() to help constructing a string in pieces.
static void lzma_attribute((format(printf, 3, 4)))
my_snprintf(char **pos, size_t *left, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	const int len = vsnprintf(*pos, *left, fmt, ap);
	va_end(ap);

	// If an error occurred, we want the caller to think that the whole
	// buffer was used. This way no more data will be written to the
	// buffer. We don't need better error handling here.
	if (len < 0 || (size_t)(len) >= *left) {
		*left = 0;
	} else {
		*pos += len;
		*left -= len;
	}

	return;
}


extern void
message_init(const char *given_argv0)
{
	// Name of the program
	argv0 = given_argv0;

	// If --verbose is used, we use a progress indicator if and only
	// if stderr is a terminal. If stderr is not a terminal, we print
	// verbose information only after finishing the file. As a special
	// exception, even if --verbose was not used, user can send SIGALRM
	// to make us print progress information once without automatic
	// updating.
	progress_automatic = isatty(STDERR_FILENO);

	// Commented out because COLUMNS is rarely exported to environment.
	// Most users have at least 80 columns anyway, let's think something
	// fancy here if enough people complain.
/*
	if (progress_automatic) {
		// stderr is a terminal. Check the COLUMNS environment
		// variable to see if the terminal is wide enough. If COLUMNS
		// doesn't exist or it has some unparseable value, we assume
		// that the terminal is wide enough.
		const char *columns_str = getenv("COLUMNS");
		if (columns_str != NULL) {
			char *endptr;
			const long columns = strtol(columns_str, &endptr, 10);
			if (*endptr != '\0' || columns < 80)
				progress_automatic = false;
		}
	}
*/

#ifdef _WIN32
	timer_queue = CreateTimerQueue();
#else
#	ifndef SA_RESTART
#		define SA_RESTART 0
#	endif
	// Establish the signal handler for SIGALRM. Since this signal
	// doesn't require any quick action, we set SA_RESTART.
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &progress_signal_handler;
	if (sigaction(SIGALRM, &sa, NULL))
		message_signal_handler();
#endif

	return;
}


extern void
message_verbosity_increase(void)
{
	if (verbosity < V_DEBUG)
		++verbosity;

	return;
}


extern void
message_verbosity_decrease(void)
{
	if (verbosity > V_SILENT)
		--verbosity;

	return;
}


extern void
message_set_files(unsigned int files)
{
	files_total = files;
	return;
}


/// Prints the name of the current file if it hasn't been printed already,
/// except if we are processing exactly one stream from stdin to stdout.
/// I think it looks nicer to not print "(stdin)" when --verbose is used
/// in a pipe and no other files are processed.
static void
print_filename(void)
{
	if (!current_filename_printed
			&& (files_total != 1 || filename != stdin_filename)) {
		signals_block();

		// If a file was already processed, put an empty line
		// before the next filename to improve readability.
		if (first_filename_printed)
			fputc('\n', stderr);

		first_filename_printed = true;
		current_filename_printed = true;

		// If we don't know how many files there will be due
		// to usage of --files or --files0.
		if (files_total == 0)
			fprintf(stderr, "%s (%u)\n", filename,
					files_pos);
		else
			fprintf(stderr, "%s (%u/%u)\n", filename,
					files_pos, files_total);

		signals_unblock();
	}

	return;
}


extern void
message_progress_start(const char *src_name, uint64_t in_size)
{
	// Store the processing start time of the file and its expected size.
	// If we aren't printing any statistics, then these are unused. But
	// since it is possible that the user tells us with SIGALRM to show
	// statistics, we need to have these available anyway.
	start_time = my_time();
	filename = src_name;
	expected_in_size = in_size;

	// Indicate the name of this file hasn't been printed to
	// stderr yet.
	current_filename_printed = false;

	// Start numbering the files starting from one.
	++files_pos;

	// If progress indicator is wanted, print the filename and possibly
	// the file count now. As an exception, if there is exactly one file,
	// do not print the filename at all.
	if (verbosity >= V_VERBOSE && progress_automatic) {
		// Print the filename to stderr if that is appropriate with
		// the current settings.
		print_filename();

		// Start the timer to set progress_needs_updating to true
		// after about one second. An alternative would to be set
		// progress_needs_updating to true here immediatelly, but
		// setting the timer looks better to me, since extremely
		// early progress info is pretty much useless.
		my_alarm(1);
	}

	return;
}


/// Make the string indicating completion percentage.
static const char *
progress_percentage(uint64_t in_pos)
{
	// If the size of the input file is unknown or the size told us is
	// clearly wrong since we have processed more data than the alleged
	// size of the file, show a static string indicating that we have
	// no idea of the completion percentage.
	if (expected_in_size == 0 || in_pos > expected_in_size)
		return "--- %";

	static char buf[sizeof("99.9 %")];

	// Never show 100.0 % before we actually are finished (that case is
	// handled separately in message_progress_end()).
	snprintf(buf, sizeof(buf), "%.1f %%",
			(double)(in_pos) / (double)(expected_in_size) * 99.9);

	return buf;
}


static void
progress_sizes_helper(char **pos, size_t *left, uint64_t value, bool final)
{
	if (final) {
		// At maximum of four digits is allowed for exact byte count.
		if (value < 10000) {
			my_snprintf(pos, left, "%'" PRIu64 " B", value);
			return;
		}

		// At maximum of five significant digits is allowed for KiB.
		if (value < UINT64_C(10239900)) {
			my_snprintf(pos, left, "%'.1f KiB",
					(double)(value) / 1024.0);
			return;
		}
	}

	// Otherwise we use MiB.
	my_snprintf(pos, left, "%'.1f MiB",
			(double)(value) / (1024.0 * 1024.0));
	return;
}


/// Make the string containing the amount of input processed, amount of
/// output produced, and the compression ratio.
static const char *
progress_sizes(uint64_t compressed_pos, uint64_t uncompressed_pos, bool final)
{
	// This is enough to hold sizes up to about 99 TiB if thousand
	// separator is used, or about 1 PiB without thousand separator.
	// After that the progress indicator will look a bit silly, since
	// the compression ratio no longer fits with three decimal places.
	static char buf[44];

	char *pos = buf;
	size_t left = sizeof(buf);

	// Print the sizes. If this the final message, use more reasonable
	// units than MiB if the file was small.
	progress_sizes_helper(&pos, &left, compressed_pos, final);
	my_snprintf(&pos, &left, " / ");
	progress_sizes_helper(&pos, &left, uncompressed_pos, final);

	// Avoid division by zero. If we cannot calculate the ratio, set
	// it to some nice number greater than 10.0 so that it gets caught
	// in the next if-clause.
	const double ratio = uncompressed_pos > 0
			? (double)(compressed_pos) / (double)(uncompressed_pos)
			: 16.0;

	// If the ratio is very bad, just indicate that it is greater than
	// 9.999. This way the length of the ratio field stays fixed.
	if (ratio > 9.999)
		snprintf(pos, left, " > %.3f", 9.999);
	else
		snprintf(pos, left, " = %.3f", ratio);

	return buf;
}


/// Make the string containing the processing speed of uncompressed data.
static const char *
progress_speed(uint64_t uncompressed_pos, double elapsed)
{
	// Don't print the speed immediatelly, since the early values look
	// like somewhat random.
	if (elapsed < 3.0)
		return "";

	static const char unit[][8] = {
		"KiB/s",
		"MiB/s",
		"GiB/s",
	};

	size_t unit_index = 0;

	// Calculate the speed as KiB/s.
	double speed = (double)(uncompressed_pos) / (elapsed * 1024.0);

	// Adjust the unit of the speed if needed.
	while (speed > 999.9) {
		speed /= 1024.0;
		if (++unit_index == ARRAY_SIZE(unit))
			return ""; // Way too fast ;-)
	}

	static char buf[sizeof("999.9 GiB/s")];
	snprintf(buf, sizeof(buf), "%.1f %s", speed, unit[unit_index]);
	return buf;
}


/// Make a string indicating elapsed or remaining time. The format is either
/// M:SS or H:MM:SS depending on if the time is an hour or more.
static const char *
progress_time(uint32_t seconds)
{
	// 9999 hours = 416 days
	static char buf[sizeof("9999:59:59")];

	// Don't show anything if the time is zero or ridiculously big.
	if (seconds == 0 || seconds > ((UINT32_C(9999) * 60) + 59) * 60 + 59)
		return "";

	uint32_t minutes = seconds / 60;
	seconds %= 60;

	if (minutes >= 60) {
		const uint32_t hours = minutes / 60;
		minutes %= 60;
		snprintf(buf, sizeof(buf),
				"%" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
				hours, minutes, seconds);
	} else {
		snprintf(buf, sizeof(buf), "%" PRIu32 ":%02" PRIu32,
				minutes, seconds);
	}

	return buf;
}


/// Make the string to contain the estimated remaining time, or if the amount
/// of input isn't known, how much time has elapsed.
static const char *
progress_remaining(uint64_t in_pos, double elapsed)
{
	// If we don't know the size of the input, we indicate the time
	// spent so far.
	if (expected_in_size == 0 || in_pos > expected_in_size)
		return progress_time((uint32_t)(elapsed));

	// If we are at the very beginning of the file or the file is very
	// small, don't give any estimate to avoid far too wrong estimations.
	if (in_pos < (UINT64_C(1) << 19) || elapsed < 8.0)
		return "";

	// Calculate the estimate. Don't give an estimate of zero seconds,
	// since it is possible that all the input has been already passed
	// to the library, but there is still quite a bit of output pending.
	uint32_t remaining = (double)(expected_in_size - in_pos)
			* elapsed / (double)(in_pos);
	if (remaining == 0)
		remaining = 1;

	return progress_time(remaining);
}


extern void
message_progress_update(uint64_t in_pos, uint64_t out_pos)
{
	// If there's nothing to do, return immediatelly.
	if (!progress_needs_updating || in_pos == 0)
		return;

	// Print the filename if it hasn't been printed yet.
	print_filename();

	// Calculate how long we have been processing this file.
	const double elapsed = my_time() - start_time;

	// Set compressed_pos and uncompressed_pos.
	uint64_t compressed_pos;
	uint64_t uncompressed_pos;
	if (opt_mode == MODE_COMPRESS) {
		compressed_pos = out_pos;
		uncompressed_pos = in_pos;
	} else {
		compressed_pos = in_pos;
		uncompressed_pos = out_pos;
	}

	signals_block();

	// Print the actual progress message. The idea is that there is at
	// least three spaces between the fields in typical situations, but
	// even in rare situations there is at least one space.
	fprintf(stderr, "  %7s %43s   %11s %10s\r",
		progress_percentage(in_pos),
		progress_sizes(compressed_pos, uncompressed_pos, false),
		progress_speed(uncompressed_pos, elapsed),
		progress_remaining(in_pos, elapsed));

	// Updating the progress info was finished. Reset
	// progress_needs_updating to wait for the next SIGALRM.
	//
	// NOTE: This has to be done before my_alarm() call or with (very) bad
	// luck we could be setting this to false after the alarm has already
	// been triggered.
	progress_needs_updating = false;

	if (progress_automatic) {
		// Mark that the progress indicator is active, so if an error
		// occurs, the error message gets printed cleanly.
		progress_active = true;

		// Restart the timer so that progress_needs_updating gets
		// set to true after about one second.
		my_alarm(1);
	} else {
		// The progress message was printed because user had sent us
		// SIGALRM. In this case, each progress message is printed
		// on its own line.
		fputc('\n', stderr);
	}

	signals_unblock();

	return;
}


extern void
message_progress_end(uint64_t in_pos, uint64_t out_pos, bool success)
{
	// If we are not in verbose mode, we have nothing to do.
	if (verbosity < V_VERBOSE || user_abort)
		return;

	// Cancel a pending alarm, if any.
	if (progress_automatic) {
		my_alarm(0);
		progress_active = false;
	}

	const double elapsed = my_time() - start_time;

	uint64_t compressed_pos;
	uint64_t uncompressed_pos;
	if (opt_mode == MODE_COMPRESS) {
		compressed_pos = out_pos;
		uncompressed_pos = in_pos;
	} else {
		compressed_pos = in_pos;
		uncompressed_pos = out_pos;
	}

	// If it took less than a second, don't display the time.
	const char *elapsed_str = progress_time((double)(elapsed));

	signals_block();

	// When using the auto-updating progress indicator, the final
	// statistics are printed in the same format as the progress
	// indicator itself.
	if (progress_automatic && in_pos > 0) {
		// Using floating point conversion for the percentage instead
		// of static "100.0 %" string, because the decimal separator
		// isn't a dot in all locales.
		fprintf(stderr, "  %5.1f %% %43s   %11s %10s\n",
			100.0,
			progress_sizes(compressed_pos, uncompressed_pos, true),
			progress_speed(uncompressed_pos, elapsed),
			elapsed_str);

	// When no automatic progress indicator is used, don't print a verbose
	// message at all if we something went wrong and we couldn't produce
	// any output. If we did produce output, then it is sometimes useful
	// to tell that to the user, especially if we detected an error after
	// a time-consuming operation.
	} else if (success || out_pos > 0) {
		// The filename and size information are always printed.
		fprintf(stderr, "%s: %s", filename, progress_sizes(
				compressed_pos, uncompressed_pos, true));

		// The speed and elapsed time aren't always shown.
		const char *speed = progress_speed(uncompressed_pos, elapsed);
		if (speed[0] != '\0')
			fprintf(stderr, ", %s", speed);

		if (elapsed_str[0] != '\0')
			fprintf(stderr, ", %s", elapsed_str);

		fputc('\n', stderr);
	}

	signals_unblock();

	return;
}


static void
vmessage(enum message_verbosity v, const char *fmt, va_list ap)
{
	if (v <= verbosity) {
		signals_block();

		// If there currently is a progress message on the screen,
		// print a newline so that the progress message is left
		// readable. This is good, because it is nice to be able to
		// see where the error occurred. (The alternative would be
		// to clear the progress message and replace it with the
		// error message.)
		if (progress_active) {
			progress_active = false;
			fputc('\n', stderr);
		}

		fprintf(stderr, "%s: ", argv0);
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);

		signals_unblock();
	}

	return;
}


extern void
message(enum message_verbosity v, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(v, fmt, ap);
	va_end(ap);
	return;
}


extern void
message_warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_WARNING, fmt, ap);
	va_end(ap);

	set_exit_status(E_WARNING);
	return;
}


extern void
message_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_ERROR, fmt, ap);
	va_end(ap);

	set_exit_status(E_ERROR);
	return;
}


extern void
message_fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_ERROR, fmt, ap);
	va_end(ap);

	my_exit(E_ERROR);
}


extern void
message_bug(void)
{
	message_fatal(_("Internal error (bug)"));
}


extern void
message_signal_handler(void)
{
	message_fatal(_("Cannot establish signal handlers"));
}


extern const char *
message_strm(lzma_ret code)
{
	switch (code) {
	case LZMA_NO_CHECK:
		return _("No integrity check; not verifying file integrity");

	case LZMA_UNSUPPORTED_CHECK:
		return _("Unsupported type of integrity check; "
				"not verifying file integrity");

	case LZMA_MEM_ERROR:
		return strerror(ENOMEM);

	case LZMA_MEMLIMIT_ERROR:
		return _("Memory usage limit reached");

	case LZMA_FORMAT_ERROR:
		return _("File format not recognized");

	case LZMA_OPTIONS_ERROR:
		return _("Unsupported options");

	case LZMA_DATA_ERROR:
		return _("Compressed data is corrupt");

	case LZMA_BUF_ERROR:
		return _("Unexpected end of input");

	case LZMA_OK:
	case LZMA_STREAM_END:
	case LZMA_GET_CHECK:
	case LZMA_PROG_ERROR:
		return _("Internal error (bug)");
	}

	return NULL;
}


extern void
message_filters(enum message_verbosity v, const lzma_filter *filters)
{
	if (v > verbosity)
		return;

	fprintf(stderr, _("%s: Filter chain:"), argv0);

	for (size_t i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i) {
		fprintf(stderr, " --");

		switch (filters[i].id) {
		case LZMA_FILTER_LZMA1:
		case LZMA_FILTER_LZMA2: {
			const lzma_options_lzma *opt = filters[i].options;
			const char *mode;
			const char *mf;

			switch (opt->mode) {
			case LZMA_MODE_FAST:
				mode = "fast";
				break;

			case LZMA_MODE_NORMAL:
				mode = "normal";
				break;

			default:
				mode = "UNKNOWN";
				break;
			}

			switch (opt->mf) {
			case LZMA_MF_HC3:
				mf = "hc3";
				break;

			case LZMA_MF_HC4:
				mf = "hc4";
				break;

			case LZMA_MF_BT2:
				mf = "bt2";
				break;

			case LZMA_MF_BT3:
				mf = "bt3";
				break;

			case LZMA_MF_BT4:
				mf = "bt4";
				break;

			default:
				mf = "UNKNOWN";
				break;
			}

			fprintf(stderr, "lzma%c=dict=%" PRIu32
					",lc=%" PRIu32 ",lp=%" PRIu32
					",pb=%" PRIu32
					",mode=%s,nice=%" PRIu32 ",mf=%s"
					",depth=%" PRIu32,
					filters[i].id == LZMA_FILTER_LZMA2
						? '2' : '1',
					opt->dict_size,
					opt->lc, opt->lp, opt->pb,
					mode, opt->nice_len, mf, opt->depth);
			break;
		}

		case LZMA_FILTER_X86:
			fprintf(stderr, "x86");
			break;

		case LZMA_FILTER_POWERPC:
			fprintf(stderr, "powerpc");
			break;

		case LZMA_FILTER_IA64:
			fprintf(stderr, "ia64");
			break;

		case LZMA_FILTER_ARM:
			fprintf(stderr, "arm");
			break;

		case LZMA_FILTER_ARMTHUMB:
			fprintf(stderr, "armthumb");
			break;

		case LZMA_FILTER_SPARC:
			fprintf(stderr, "sparc");
			break;

		case LZMA_FILTER_DELTA: {
			const lzma_options_delta *opt = filters[i].options;
			fprintf(stderr, "delta=dist=%" PRIu32, opt->dist);
			break;
		}

		default:
			fprintf(stderr, "UNKNOWN");
			break;
		}
	}

	fputc('\n', stderr);
	return;
}


extern void
message_try_help(void)
{
	// Print this with V_WARNING instead of V_ERROR to prevent it from
	// showing up when --quiet has been specified.
	message(V_WARNING, _("Try `%s --help' for more information."), argv0);
	return;
}


extern void
message_version(void)
{
	// It is possible that liblzma version is different than the command
	// line tool version, so print both.
	printf("xz " LZMA_VERSION_STRING "\n");
	printf("liblzma %s\n", lzma_version_string());
	my_exit(E_SUCCESS);
}


extern void
message_help(bool long_help)
{
	printf(_("Usage: %s [OPTION]... [FILE]...\n"
			"Compress or decompress FILEs in the .xz format.\n\n"),
			argv0);

	puts(_("Mandatory arguments to long options are mandatory for "
			"short options too.\n"));

	if (long_help)
		puts(_(" Operation mode:\n"));

	puts(_(
"  -z, --compress      force compression\n"
"  -d, --decompress    force decompression\n"
"  -t, --test          test compressed file integrity\n"
"  -l, --list          list information about files"));

	if (long_help)
		puts(_("\n Operation modifiers:\n"));

	puts(_(
"  -k, --keep          keep (don't delete) input files\n"
"  -f, --force         force overwrite of output file and (de)compress links\n"
"  -c, --stdout        write to standard output and don't delete input files"));

	if (long_help)
		puts(_(
"  -S, --suffix=.SUF   use the suffix `.SUF' on compressed files\n"
"      --files=[FILE]  read filenames to process from FILE; if FILE is\n"
"                      omitted, filenames are read from the standard input;\n"
"                      filenames must be terminated with the newline character\n"
"      --files0=[FILE] like --files but use the null character as terminator"));

	if (long_help) {
		puts(_("\n Basic file format and compression options:\n"));
		puts(_(
"  -F, --format=FMT    file format to encode or decode; possible values are\n"
"                      `auto' (default), `xz', `lzma', and `raw'\n"
"  -C, --check=CHECK   integrity check type: `crc32', `crc64' (default),\n"
"                      or `sha256'"));
	}

	puts(_(
"  -0 .. -9            compression preset; 0-2 fast compression, 3-5 good\n"
"                      compression, 6-9 excellent compression; default is 6"));

	puts(_(
"  -M, --memory=NUM    use roughly NUM bytes of memory at maximum; 0 indicates\n"
"                      the default setting, which depends on the operation mode\n"
"                      and the amount of physical memory (RAM)"));

	if (long_help) {
		puts(_(
"\n Custom filter chain for compression (alternative for using presets):"));

#if defined(HAVE_ENCODER_LZMA1) || defined(HAVE_DECODER_LZMA1) \
		|| defined(HAVE_ENCODER_LZMA2) || defined(HAVE_DECODER_LZMA2)
		puts(_(
"\n"
"  --lzma1=[OPTS]      LZMA1 or LZMA2; OPTS is a comma-separated list of zero or\n"
"  --lzma2=[OPTS]      more of the following options (valid values; default):\n"
"                        preset=NUM reset options to preset number NUM (1-9)\n"
"                        dict=NUM   dictionary size (4KiB - 1536MiB; 8MiB)\n"
"                        lc=NUM     number of literal context bits (0-4; 3)\n"
"                        lp=NUM     number of literal position bits (0-4; 0)\n"
"                        pb=NUM     number of position bits (0-4; 2)\n"
"                        mode=MODE  compression mode (fast, normal; normal)\n"
"                        nice=NUM   nice length of a match (2-273; 64)\n"
"                        mf=NAME    match finder (hc3, hc4, bt2, bt3, bt4; bt4)\n"
"                        depth=NUM  maximum search depth; 0=automatic (default)"));
#endif

		puts(_(
"\n"
"  --x86               x86 filter (sometimes called BCJ filter)\n"
"  --powerpc           PowerPC (big endian) filter\n"
"  --ia64              IA64 (Itanium) filter\n"
"  --arm               ARM filter\n"
"  --armthumb          ARM-Thumb filter\n"
"  --sparc             SPARC filter"));

#if defined(HAVE_ENCODER_DELTA) || defined(HAVE_DECODER_DELTA)
		puts(_(
"\n"
"  --delta=[OPTS]      Delta filter; valid OPTS (valid values; default):\n"
"                        dist=NUM   distance between bytes being subtracted\n"
"                                   from each other (1-256; 1)"));
#endif

#if defined(HAVE_ENCODER_SUBBLOCK) || defined(HAVE_DECODER_SUBBLOCK)
		puts(_(
"\n"
"  --subblock=[OPTS]   Subblock filter; valid OPTS (valid values; default):\n"
"                        size=NUM   number of bytes of data per subblock\n"
"                                   (1 - 256Mi; 4Ki)\n"
"                        rle=NUM    run-length encoder chunk size (0-256; 0)"));
#endif
	}

	if (long_help)
		puts(_("\n Other options:\n"));

	puts(_(
"  -q, --quiet         suppress warnings; specify twice to suppress errors too\n"
"  -v, --verbose       be verbose; specify twice for even more verbose"));

	if (long_help)
		puts(_(
"\n"
"  -h, --help          display the short help (lists only the basic options)\n"
"  -H, --long-help     display this long help"));
	else
		puts(_(
"  -h, --help          display this short help\n"
"  -H, --long-help     display the long help (lists also the advanced options)"));

	puts(_(
"  -V, --version       display the version number"));

	puts(_("\nWith no FILE, or when FILE is -, read standard input.\n"));

	if (long_help) {
		printf(_(
"On this system and configuration, the tool will use at maximum of\n"
"  * roughly %'" PRIu64 " MiB RAM for compression;\n"
"  * roughly %'" PRIu64 " MiB RAM for decompression; and\n"),
				hardware_memlimit_encoder() / (1024 * 1024),
				hardware_memlimit_decoder() / (1024 * 1024));
		printf(N_("  * one thread for (de)compression.\n\n",
			"  * %'" PRIu64 " threads for (de)compression.\n\n",
			(uint64_t)(opt_threads)), (uint64_t)(opt_threads));
	}

	printf(_("Report bugs to <%s> (in English or Finnish).\n"),
			PACKAGE_BUGREPORT);

	my_exit(E_SUCCESS);
}
