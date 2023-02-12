///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.c
/// \brief      main()
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include <ctype.h>

// An attempt at being portable with different path separators.
#ifdef TUKLIB_DOSLIKE
#	define PATH_SEP '\\'
#else
#	define PATH_SEP '/'
#endif


/// Exit status to use. This can be changed with set_exit_status().
static enum exit_status_type exit_status = E_SUCCESS;

#if defined(_WIN32) && !defined(__CYGWIN__)
/// exit_status has to be protected with a critical section due to
/// how "signal handling" is done on Windows. See signals.c for details.
static CRITICAL_SECTION exit_status_cs;
#endif

/// True if --no-warn is specified. When this is true, we don't set
/// the exit status to E_WARNING when something worth a warning happens.
static bool no_warn = false;


extern void
set_exit_status(enum exit_status_type new_status)
{
	assert(new_status == E_WARNING || new_status == E_ERROR);

#if defined(_WIN32) && !defined(__CYGWIN__)
	EnterCriticalSection(&exit_status_cs);
#endif

	if (exit_status != E_ERROR)
		exit_status = new_status;

#if defined(_WIN32) && !defined(__CYGWIN__)
	LeaveCriticalSection(&exit_status_cs);
#endif

	return;
}


extern void
set_exit_no_warn(void)
{
	no_warn = true;
	return;
}


static const char *
read_name(const args_info *args)
{
	// FIXME: Maybe we should have some kind of memory usage limit here
	// like the tool has for the actual compression and decompression.
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

// Prototype the process_entry() function since process_directory() and
// process_entry() recursively call each other.
static void process_entry(const char *path);


// Process an entire directory in recursive mode.
static void
process_directory(const char* path)
{
	// Read all entries into a character array. Individual entries
	// are processed after reading all entries to prevent the system
	// from running out of file descriptors in the case of deep recursion.
	char *dir_str = dir_to_stream(path);
	if (dir_str == NULL)
		return;

	// Loop through entries, extend by path, and call process_entry().
	size_t entry_len = 0;
	const size_t path_len = strlen(path);

	for (char *entry = dir_str; *entry; entry += entry_len + 1) {
		entry_len = strlen(entry);

		// Skip any empty entries.
		if (entry_len == 0)
			continue;

		// Ignore all files begining with "." when recursing in
		// compression mode unless the --keep option is used.
		// It could cause problems if the user accidentally deletes
		// certain "." files while recursing.
		// (ssh keys, git files, bash files, etc.)
		//
		// If --keep is not used, then only "." and ".." need to be
		// ignored.
		if ((!opt_keep_original && opt_mode == MODE_COMPRESS
				&& entry[0] == '.')
				|| !strcmp(entry, ".")
				|| !strcmp(entry, ".."))
			continue;

		const size_t new_path_length = entry_len + path_len + 2;

		// Check for overflow on the path length
		if (new_path_length < path_len)
			continue;

		// Allocate the path dynamically instead of trying to guess
		// the max path length on the platform. Also, it could lead
		// to a lot of wasted stack space if the recursion is deep.
		char *new_path = xmalloc(new_path_length);

		// Copy the path of the directory
		memcpy(new_path, path, path_len);

		// Add separator for new entry in the directory
		new_path[path_len] = PATH_SEP;

		// Copy the next entry name
		memcpy(new_path + (path_len + 1), entry, entry_len);

		// NULL terminate the string
		new_path[path_len + entry_len + 1] = '\0';

		process_entry(new_path);

		free(new_path);
	}

	free(dir_str);
}


// Process a single entry specified by the user at the command line,
// standard in, or from a file when --files or --files0 is used.
// If recursive mode is used, the entry can be either a file or a directory.
// If recursive mode is not used, it will return after the failed call to
// io_open_src() if the entry is a directory.
static void
process_entry(const char *path)
{
	// Set and possibly print the filename for the progress message.
	message_filename(path);

	// Open the entry
	file_pair *pair = io_open_src(path);
	if (pair == NULL)
		return;

	// pair->directory can only be true if --recursive mode is used.
	if (pair->directory) {
		process_directory(path);
		return;
	}

#ifdef HAVE_DECODERS
	if (opt_mode == MODE_LIST) {
		if (path == stdin_filename) {
			message_error(_("--list does not support reading from "
					"standard input"));
			return;
		}

		list_file(pair);
		return;
	}
#endif

	coder_run(pair);
}


int
main(int argc, char **argv)
{
#ifdef HAVE_PLEDGE
	// OpenBSD's pledge(2) sandbox
	//
	// Unconditionally enable sandboxing with fairly relaxed promises.
	// This is still way better than having no sandbox at all. :-)
	// More strict promises will be made later in file_io.c if possible.
	if (pledge("stdio rpath wpath cpath fattr", "")) {
		// Don't translate the string or use message_fatal() as
		// those haven't been initialized yet.
		fprintf(stderr, "%s: Failed to enable the sandbox\n", argv[0]);
		return E_ERROR;
	}
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
	InitializeCriticalSection(&exit_status_cs);
#endif

	// Set up the progname variable.
	tuklib_progname_init(argv);

	// Initialize the file I/O. This makes sure that
	// stdin, stdout, and stderr are something valid.
	io_init();

	// Set up the locale and message translations.
	tuklib_gettext_init(PACKAGE, LOCALEDIR);

	// Initialize handling of error/warning/other messages.
	message_init();

	// Set hardware-dependent default values. These can be overridden
	// on the command line, thus this must be done before args_parse().
	hardware_init();

	// Parse the command line arguments and get an array of filenames.
	// This doesn't return if something is wrong with the command line
	// arguments. If there are no arguments, one filename ("-") is still
	// returned to indicate stdin.
	args_info args;
	args_parse(&args, argc, argv);

	if (opt_mode != MODE_LIST && opt_robot)
		message_fatal(_("Compression and decompression with --robot "
			"are not supported yet."));

	// Tell the message handling code how many input files there are if
	// we know it. This way the progress indicator can show it.
	if (args.files_name != NULL || opt_recursive)
		message_set_files(0);
	else
		message_set_files(args.arg_count);

	// Refuse to write compressed data to standard output if it is
	// a terminal.
	if (opt_mode == MODE_COMPRESS) {
		if (opt_stdout || (args.arg_count == 1
				&& strcmp(args.arg_names[0], "-") == 0)) {
			if (is_tty_stdout()) {
				message_try_help();
				tuklib_exit(E_ERROR, E_ERROR, false);
			}
		}
	}

	// Set up the signal handlers. We don't need these before we
	// start the actual action and not in --list mode, so this is
	// done after parsing the command line arguments.
	//
	// It's good to keep signal handlers in normal compression and
	// decompression modes even when only writing to stdout, because
	// we might need to restore O_APPEND flag on stdout before exiting.
	// In --test mode, signal handlers aren't really needed, but let's
	// keep them there for consistency with normal decompression.
	if (opt_mode != MODE_LIST)
		signals_init();

#ifdef ENABLE_SANDBOX
	// Set a flag that sandboxing is allowed if all these are true:
	//   - --files or --files0 wasn't used.
	//   - There is exactly one input file or we are reading from stdin.
	//   - We won't create any files: output goes to stdout or --test
	//     or --list was used. Note that --test implies opt_stdout = true
	//     but --list doesn't.
	//
	// This is obviously not ideal but it was easy to implement and
	// it covers the most common use cases.
	//
	// TODO: Make sandboxing work for other situations too.
	if (args.files_name == NULL && args.arg_count == 1
			&& (opt_stdout || strcmp("-", args.arg_names[0]) == 0
				|| opt_mode == MODE_LIST))
		io_allow_sandbox();
#endif

#ifdef HAVE_DECODERS
	if (opt_mode == MODE_LIST) {
		if (opt_format != FORMAT_XZ && opt_format != FORMAT_AUTO)
			message_fatal(_("--list works only on .xz files "
					"(--format=xz or --format=auto)"));

		// Unset opt_stdout so that io_open_src() won't accept
		// special files. Set opt_force so that io_open_src() will
		// follow symlinks.
		opt_stdout = false;
		opt_force = true;
	}
#endif

	// Process the files given on the command line. Note that if no names
	// were given, args_parse() gave us a fake "-" filename.
	for (unsigned i = 0; i < args.arg_count && !user_abort; ++i) {
		if (strcmp("-", args.arg_names[i]) == 0) {
			// Processing from stdin to stdout. Check that we
			// aren't writing compressed data to a terminal or
			// reading it from a terminal.
			if (opt_mode == MODE_COMPRESS) {
				if (is_tty_stdout())
					continue;
			} else if (is_tty_stdin()) {
				continue;
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
			// recognized by coder_run() and other things.
			// This way error messages get a proper filename
			// string and the code still knows that it is
			// handling the special case of stdin.
			args.arg_names[i] = (char *)stdin_filename;
		}

		// Do the actual compression or decompression.
		process_entry(args.arg_names[i]);
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
			process_entry(name);
		}

		if (args.files_name != stdin_filename)
			(void)fclose(args.files_file);
	}

#ifdef HAVE_DECODERS
	// All files have now been handled. If in --list mode, display
	// the totals before exiting. We don't have signal handlers
	// enabled in --list mode, so we don't need to check user_abort.
	if (opt_mode == MODE_LIST) {
		assert(!user_abort);
		list_totals();
	}
#endif

#ifndef NDEBUG
	coder_free();
	args_free();
#endif

	// If we have got a signal, raise it to kill the program instead
	// of calling tuklib_exit().
	signals_exit();

	// Make a local copy of exit_status to keep the Windows code
	// thread safe. At this point it is fine if we miss the user
	// pressing C-c and don't set the exit_status to E_ERROR on
	// Windows.
#if defined(_WIN32) && !defined(__CYGWIN__)
	EnterCriticalSection(&exit_status_cs);
#endif

	enum exit_status_type es = exit_status;

#if defined(_WIN32) && !defined(__CYGWIN__)
	LeaveCriticalSection(&exit_status_cs);
#endif

	// Suppress the exit status indicating a warning if --no-warn
	// was specified.
	if (es == E_WARNING && no_warn)
		es = E_SUCCESS;

	tuklib_exit((int)es, E_ERROR, message_verbosity_get() != V_SILENT);
}
