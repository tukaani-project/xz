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

// prctl(PR_SET_NO_NEW_PRIVS, ...) is required with Landlock but it can be
// activated even when conditions for strict sandboxing aren't met.
#ifdef HAVE_LINUX_LANDLOCK_H
#	include <sys/prctl.h>
#endif

/// The directory_list type is used in recursive mode to keep track of all
/// the directories that need processing. Its used a a queue to process
/// directories in the order they are discovered. Files, on the other hand
/// are processed right away to reduce the size of the queue and hence the
/// amount of memory needed to be allocated at any one time.
typedef struct directory_list_s {
	/// Path to the directory. This is used as a pointer since it is
	/// likely that most directories do not need the full possible file
	/// path length allowed by systems. This saves memory in cases where
	/// many directories need to be on the queue at the same time.
	char *dir_path;

	/// Pointer to the next directory in the queue. This is only a
	/// singly linked list since we only ever need to process the queue
	/// in one direction.
	struct directory_list_s *next;
} directory_list;


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
					"to use '--files0' instead "
					"of '--files'?"), args->files_name);
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


static void
process_entry(const char *path)
{
#ifdef HAVE_DECODERS
	if (opt_mode == MODE_LIST && path == stdin_filename) {
		message_error(_("--list does not support reading from "
				"standard input"));
		return;
	}
#endif

	// Open the entry
	file_pair *pair = io_open_src(path);
	if (pair == NULL)
		return;

#if defined(_MSC_VER) || defined(HAVE_DIRENT_H)
	// io_open_src() will return NULL if the path points to a directory
	// and we aren't in recursive mode. So there is no need to check
	// for recursive mode here.
	if (pair->is_directory) {
		// Create the queue of directories to process. The first
		// item in the queue will be the base entry. The first item
		// is dynamically allocated to simplify the memory freeing
		// code later on.
		directory_list *dir_list = xmalloc(sizeof(directory_list));

		dir_list->dir_path = xstrdup(path);

		// Strip any trailing path separators at the end of the
		// directory. This makes the path compatible with Windows
		// MSVC search functions and makes the output look nicer.
		for (size_t i = strlen(path) - 1; dir_list->dir_path[i]
				== PATH_SEP && i > 1; i--) {
			dir_list->dir_path[i] = '\0';
		}

		dir_list->next = NULL;

		// The current pointer represents the directory we are
		// currently processing. To start, it is initialzed as the
		// base entry.
		directory_list *current = dir_list;

		// The pointer to the last item in the queue is used to
		// append new directories.
		directory_list *last = dir_list;
		do {
			directory_list* next;

			// The iterator initialization will return NULL and
			// print an error message if there is any kind of
			// problem. In this case, we can simply continue on
			// to the next directory to process.
			directory_iter *iter = directory_iterator_init(
					current->dir_path);

			// The error message is printed during
			// directory_iterator_init(), so no need to print
			// anything before proceeding to the next iteration.
			if (iter == NULL)
				goto next_iteration;

			const size_t dir_path_len = strlen(current->dir_path);

			// Set ENTRY_LEN_MAX depending on the system. On
			// POSIX systems, NAME_MAX will be defined in
			// <limit.h>. On Windows, the directory parsing
			// functions have buffers of size MAX_PATH.
#ifdef TUKLIB_DOSLIKE
#			define ENTRY_LEN_MAX MAX_PATH
#else
#			define ENTRY_LEN_MAX NAME_MAX
#endif
			char entry[ENTRY_LEN_MAX + 1];
			size_t entry_len;

			// The entry_len must be reset each iteration because
			// directory_iter_next() will only write to the entry
			// buffer if it can write the entire entry name. If the
			// value is not reset each time, it will limit the
			// next entry size based on the last entry's size.
			while ((entry_len = ENTRY_LEN_MAX)
					&& directory_iter_next(iter, entry,
						&entry_len)) {
				// Extend current directory path with
				// new entry.
				if (entry_len == 0)
					continue;

				// Check for '.' and '..' since there is no
				// point in processing them.
				if (entry[0] == '.' && ((entry[1] == '.'
						&& entry[2] == '\0')
						|| entry[1] == '\0'))
					continue;

				// The total entry size needs the "+2" to
				// make room for the directory path separator
				// and the NULL terminator.
				const size_t total_size = entry_len + dir_path_len + 2;
				char *entry_path = xmalloc(total_size);

				memcpy(entry_path, current->dir_path, dir_path_len);

				char *entry_copy_start = entry_path + dir_path_len;

				entry_path[dir_path_len] = PATH_SEP;
				entry_copy_start++;

				memcpy(entry_copy_start, entry, entry_len + 1);

				// Try to open the next entry. If it is a file
				// it will be processed immediately. If it is a
				// directory it will be added to the queue to
				// be processed later. Processing files right
				// away reduces the amount of memory needed
				// for queue nodes and stored file paths.
				// Exploring directories only increases the
				// amount of memory needed so its better to
				// prioritize processing files as early as
				// possible.
				pair = io_open_src(entry_path);

				if (pair == NULL) {
					free(entry_path);
					continue;
				}

				if (pair->is_directory) {
					directory_list *next_dir = xmalloc(
						sizeof(directory_list));
					next_dir->dir_path = entry_path;
					next_dir->next = NULL;
					last->next = next_dir;
					last = next_dir;
				} else if (entry[0] == '.'
						&& opt_mode == MODE_COMPRESS
						&& !opt_keep_original) {
					message_warning(_("%s: Hidden file "
						"skipped during recursive "
						"compression mode. Use --keep "
						"to process these files.\n"),
						entry_path);
					free(entry_path);
				} else {

					message_filename(entry_path);
#ifdef HAVE_DECODERS
					if (opt_mode == MODE_LIST)
						list_file(pair);
					else
#endif
					coder_run(pair);
					free(entry_path);
				}
			}

			directory_iter_close(iter);
next_iteration:
			next = current->next;

			free(current->dir_path);
			free(current);

			current = next;
		} while (current != NULL);

		return;
	}

#endif // defined(_MSC_VER) || defined(HAVE_DIRENT_H)

// Set and possibly print the filename for the progress message.
message_filename(path);

#ifdef HAVE_DECODERS
	if (opt_mode == MODE_LIST)
		list_file(pair);
	else
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

#ifdef HAVE_LINUX_LANDLOCK_H
	// Prevent the process from gaining new privileges. This must be done
	// before landlock_restrict_self(2) in file_io.c but since we will
	// never need new privileges, this call can be done here already.
	//
	// This is supported since Linux 3.5. Ignore the return value to
	// keep compatibility with old kernels. landlock_restrict_self(2)
	// will fail if the no_new_privs attribute isn't set, thus if prctl()
	// fails here the error will still be detected when it matters.
	(void)prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
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
				|| opt_mode == MODE_LIST)
				&& !opt_recursive)
		io_allow_sandbox();
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
