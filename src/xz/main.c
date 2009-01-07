///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.c
/// \brief      main()
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
#include "open_stdxxx.h"
#include <ctype.h>


volatile sig_atomic_t user_abort = false;

/// Exit status to use. This can be changed with set_exit_status().
static enum exit_status_type exit_status = E_SUCCESS;

/// If we were interrupted by a signal, we store the signal number so that
/// we can raise that signal to kill the program when all cleanups have
/// been done.
static volatile sig_atomic_t exit_signal = 0;

/// Mask of signals for which have have established a signal handler to set
/// user_abort to true.
static sigset_t hooked_signals;

/// signals_block() and signals_unblock() can be called recursively.
static size_t signals_block_count = 0;


static void
signal_handler(int sig)
{
	exit_signal = sig;
	user_abort = true;
	return;
}


static void
establish_signal_handlers(void)
{
	// List of signals for which we establish the signal handler.
	static const int sigs[] = {
		SIGINT,
		SIGTERM,
#ifdef SIGHUP
		SIGHUP,
#endif
#ifdef SIGPIPE
		SIGPIPE,
#endif
#ifdef SIGXCPU
		SIGXCPU,
#endif
#ifdef SIGXFSZ
		SIGXFSZ,
#endif
	};

	// Mask of the signals for which we have established a signal handler.
	sigemptyset(&hooked_signals);
	for (size_t i = 0; i < ARRAY_SIZE(sigs); ++i)
		sigaddset(&hooked_signals, sigs[i]);

	struct sigaction sa;

	// All the signals that we handle we also blocked while the signal
	// handler runs.
	sa.sa_mask = hooked_signals;

	// Don't set SA_RESTART, because we want EINTR so that we can check
	// for user_abort and cleanup before exiting. We block the signals
	// for which we have established a handler when we don't want EINTR.
	sa.sa_flags = 0;
	sa.sa_handler = &signal_handler;

	for (size_t i = 0; i < ARRAY_SIZE(sigs); ++i) {
		// If the parent process has left some signals ignored,
		// we don't unignore them.
		struct sigaction old;
		if (sigaction(sigs[i], NULL, &old) == 0
				&& old.sa_handler == SIG_IGN)
			continue;

		// Establish the signal handler.
		if (sigaction(sigs[i], &sa, NULL))
			message_signal_handler();
	}

	return;
}


extern void
signals_block(void)
{
	if (signals_block_count++ == 0) {
		const int saved_errno = errno;
		mythread_sigmask(SIG_BLOCK, &hooked_signals, NULL);
		errno = saved_errno;
	}

	return;
}


extern void
signals_unblock(void)
{
	assert(signals_block_count > 0);

	if (--signals_block_count == 0) {
		const int saved_errno = errno;
		mythread_sigmask(SIG_UNBLOCK, &hooked_signals, NULL);
		errno = saved_errno;
	}

	return;
}


extern void
set_exit_status(enum exit_status_type new_status)
{
	assert(new_status == E_WARNING || new_status == E_ERROR);

	if (exit_status != E_ERROR)
		exit_status = new_status;

	return;
}


extern void
my_exit(enum exit_status_type status)
{
	// Close stdout. If something goes wrong, print an error message
	// to stderr.
	{
		const int ferror_err = ferror(stdout);
		const int fclose_err = fclose(stdout);
		if (ferror_err || fclose_err) {
			// If it was fclose() that failed, we have the reason
			// in errno. If only ferror() indicated an error,
			// we have no idea what the reason was.
			message(V_ERROR, _("Writing to standard output "
					"failed: %s"),
					fclose_err ? strerror(errno)
						: _("Unknown error"));
			status = E_ERROR;
		}
	}

	// Close stderr. If something goes wrong, there's nothing where we
	// could print an error message. Just set the exit status.
	{
		const int ferror_err = ferror(stderr);
		const int fclose_err = fclose(stderr);
		if (fclose_err || ferror_err)
			status = E_ERROR;
	}

	// If we have got a signal, raise it to kill the program.
	const int sig = exit_signal;
	if (sig != 0) {
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigfillset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(sig, &sa, NULL);
		raise(exit_signal);

		// If, for some weird reason, the signal doesn't kill us,
		// we safely fall to the exit below.
	}

	exit(status);
}


static const char *
read_name(const args_info *args)
{
	// FIXME: Maybe we should have some kind of memory usage limit here
	// like the tool has for the actual compression and uncompression.
	// Giving some huge text file with --files0 makes us to read the
	// whole file in RAM.
	static char *name = NULL;
	static size_t size = 256;

	// Allocate the initial buffer. This is never freed, since after it
	// is no longer needed, the program exits very soon. It is safe to
	// use xmalloc() and xrealloc() in this function, because while
	// executing this function, no files are open for writing, and thus
	// there's no need to cleanup anything before exiting.
	if (name == NULL)
		name = xmalloc(size);

	// Write position in name
	size_t pos = 0;

	// Read one character at a time into name.
	while (!user_abort) {
		const int c = fgetc(args->files_file);

		if (ferror(args->files_file)) {
			// Take care of EINTR since we have established
			// the signal handlers already.
			if (errno == EINTR)
				continue;

			message_error(_("%s: Error reading filenames: %s"),
					args->files_name, strerror(errno));
			return NULL;
		}

		if (feof(args->files_file)) {
			if (pos != 0)
				message_error(_("%s: Unexpected end of input "
						"when reading filenames"),
						args->files_name);

			return NULL;
		}

		if (c == args->files_delim) {
			// We allow consecutive newline (--files) or '\0'
			// characters (--files0), and ignore such empty
			// filenames.
			if (pos == 0)
				continue;

			// A non-empty name was read. Terminate it with '\0'
			// and return it.
			name[pos] = '\0';
			return name;
		}

		if (c == '\0') {
			// A null character was found when using --files,
			// which expects plain text input separated with
			// newlines.
			message_error(_("%s: Null character found when "
					"reading filenames; maybe you meant "
					"to use `--files0' instead "
					"of `--files'?"), args->files_name);
			return NULL;
		}

		name[pos++] = c;

		// Allocate more memory if needed. There must always be space
		// at least for one character to allow terminating the string
		// with '\0'.
		if (pos == size) {
			size *= 2;
			name = xrealloc(name, size);
		}
	}

	return NULL;
}


int
main(int argc, char **argv)
{
	// Make sure that stdin, stdout, and and stderr are connected to
	// a valid file descriptor. Exit immediatelly with exit code ERROR
	// if we cannot make the file descriptors valid. Maybe we should
	// print an error message, but our stderr could be screwed anyway.
	open_stdxxx(E_ERROR);

	// Set up the locale.
	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	// Set up the message translations too.
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	// Set the program invocation name used in various messages, and
	// do other message handling related initializations.
	message_init(argv[0]);

	// Set hardware-dependent default values. These can be overriden
	// on the command line, thus this must be done before parse_args().
	hardware_init();

	// Parse the command line arguments and get an array of filenames.
	// This doesn't return if something is wrong with the command line
	// arguments. If there are no arguments, one filename ("-") is still
	// returned to indicate stdin.
	args_info args;
	args_parse(&args, argc, argv);

	// Tell the message handling code how many input files there are if
	// we know it. This way the progress indicator can show it.
	if (args.files_name != NULL)
		message_set_files(0);
	else
		message_set_files(args.arg_count);

	// Refuse to write compressed data to standard output if it is
	// a terminal and --force wasn't used.
	if (opt_mode == MODE_COMPRESS) {
		if (opt_stdout || (args.arg_count == 1
				&& strcmp(args.arg_names[0], "-") == 0)) {
			if (is_tty_stdout()) {
				message_try_help();
				my_exit(E_ERROR);
			}
		}
	}

	if (opt_mode == MODE_LIST) {
		message_fatal("--list is not implemented yet.");
	}

	// Hook the signal handlers. We don't need these before we start
	// the actual action, so this is done after parsing the command
	// line arguments.
	establish_signal_handlers();

	// Process the files given on the command line. Note that if no names
	// were given, parse_args() gave us a fake "-" filename.
	for (size_t i = 0; i < args.arg_count && !user_abort; ++i) {
		if (strcmp("-", args.arg_names[i]) == 0) {
			// Processing from stdin to stdout. Unless --force
			// was used, check that we aren't writing compressed
			// data to a terminal or reading it from terminal.
			if (!opt_force) {
				if (opt_mode == MODE_COMPRESS) {
					if (is_tty_stdout())
						continue;
				} else if (is_tty_stdin()) {
					continue;
				}
			}

			// It doesn't make sense to compress data from stdin
			// if we are supposed to read filenames from stdin
			// too (enabled with --files or --files0).
			if (args.files_name == stdin_filename) {
				message_error(_("Cannot read data from "
						"standard input when "
						"reading filenames "
						"from standard input"));
				continue;
			}

			// Replace the "-" with a special pointer, which is
			// recognized by process_file() and other things.
			// This way error messages get a proper filename
			// string and the code still knows that it is
			// handling the special case of stdin.
			args.arg_names[i] = (char *)stdin_filename;
		}

		// Do the actual compression or uncompression.
		process_file(args.arg_names[i]);
	}

	// If --files or --files0 was used, process the filenames from the
	// given file or stdin. Note that here we don't consider "-" to
	// indicate stdin like we do with the command line arguments.
	if (args.files_name != NULL) {
		// read_name() checks for user_abort so we don't need to
		// check it as loop termination condition.
		while (true) {
			const char *name = read_name(&args);
			if (name == NULL)
				break;

			// read_name() doesn't return empty names.
			assert(name[0] != '\0');
			process_file(name);
		}

		if (args.files_name != stdin_filename)
			(void)fclose(args.files_file);
	}

	my_exit(exit_status);
}
