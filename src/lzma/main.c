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

static sig_atomic_t exit_signal = 0;


static void
signal_handler(int sig)
{
	// FIXME Is this thread-safe together with main()?
	exit_signal = sig;

	user_abort = 1;
	return;
}


static void
establish_signal_handlers(void)
{
	struct sigaction sa;
	sa.sa_handler = &signal_handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;

	static const int sigs[] = {
		SIGHUP,
		SIGINT,
		SIGPIPE,
		SIGTERM,
		SIGXCPU,
		SIGXFSZ,
	};

	for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) {
		if (sigaction(sigs[i], &sa, NULL)) {
			errmsg(V_ERROR, _("Cannot establish signal handlers"));
			my_exit(ERROR);
		}
	}

	/*
	SIGINFO/SIGUSR1 for status reporting?
	*/
}


static bool
is_tty_stdin(void)
{
	const bool ret = isatty(STDIN_FILENO);
	if (ret) {
		// FIXME: Other threads may print between these lines.
		// Maybe that should be fixed. Not a big issue in practice.
		errmsg(V_ERROR, _("Compressed data not read from "
				"a terminal."));
		errmsg(V_ERROR, _("Use `--force' to force decompression."));
		show_try_help();
	}

	return ret;
}


static bool
is_tty_stdout(void)
{
	const bool ret = isatty(STDOUT_FILENO);
	if (ret) {
		errmsg(V_ERROR, _("Compressed data not written to "
				"a terminal."));
		errmsg(V_ERROR, _("Use `--force' to force compression."));
		show_try_help();
	}

	return ret;
}


static char *
read_name(void)
{
	size_t size = 256;
	size_t pos = 0;
	char *name = malloc(size);
	if (name == NULL) {
		out_of_memory();
		return NULL;
	}

	while (true) {
		const int c = fgetc(opt_files_file);
		if (c == EOF) {
			free(name);

			if (ferror(opt_files_file))
				errmsg(V_ERROR, _("%s: Error reading "
						"filenames: %s"),
						opt_files_name,
						strerror(errno));
			else if (pos != 0)
				errmsg(V_ERROR, _("%s: Unexpected end of "
						"input when reading "
						"filenames"), opt_files_name);

			return NULL;
		}

		if (c == '\0' || c == opt_files_split)
			break;

		name[pos++] = c;

		if (pos == size) {
			size *= 2;
			char *tmp = realloc(name, size);
			if (tmp == NULL) {
				free(name);
				out_of_memory();
				return NULL;
			}

			name = tmp;
		}
	}

	if (name != NULL)
		name[pos] = '\0';

	return name;
}


int
main(int argc, char **argv)
{
	// Make sure that stdin, stdout, and and stderr are connected to
	// a valid file descriptor. Exit immediatelly with exit code ERROR
	// if we cannot make the file descriptors valid. Maybe we should
	// print an error message, but our stderr could be screwed anyway.
	open_stdxxx(ERROR);

	// Set the program invocation name used in various messages.
	argv0 = argv[0];

	setlocale(LC_ALL, "en_US.UTF-8");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	// Set hardware-dependent default values. These can be overriden
	// on the command line, thus this must be done before parse_args().
	hardware_init();

	char **files = parse_args(argc, argv);

	if (opt_mode == MODE_COMPRESS && opt_stdout && is_tty_stdout())
		return ERROR;

	if (opt_mode == MODE_COMPRESS)
		lzma_init_encoder();
	else
		lzma_init_decoder();

	io_init();
	process_init();

	if (opt_mode == MODE_LIST) {
		errmsg(V_ERROR, "--list is not implemented yet.");
		my_exit(ERROR);
	}

	// Hook the signal handlers. We don't need these before we start
	// the actual action, so this is done after parsing the command
	// line arguments.
	establish_signal_handlers();

	while (*files != NULL && !user_abort) {
		if (strcmp("-", *files) == 0) {
			if (!opt_force) {
				if (opt_mode == MODE_COMPRESS) {
					if (is_tty_stdout()) {
						++files;
						continue;
					}
				} else if (is_tty_stdin()) {
					++files;
					continue;
				}
			}

			if (opt_files_name == stdin_filename) {
				errmsg(V_ERROR, _("Cannot read data from "
						"standard input when "
						"reading filenames "
						"from standard input"));
				++files;
				continue;
			}

			*files = (char *)stdin_filename;
		}

		process_file(*files++);
	}

	if (opt_files_name != NULL) {
		while (true) {
			char *name = read_name();
			if (name == NULL)
				break;

			if (name[0] != '\0')
				process_file(name);

			free(name);
		}

		if (opt_files_name != stdin_filename)
			(void)fclose(opt_files_file);
	}

	io_finish();

	if (exit_signal != 0) {
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigfillset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(exit_signal, &sa, NULL);
		raise(exit_signal);
	}

	my_exit(exit_status);
}
