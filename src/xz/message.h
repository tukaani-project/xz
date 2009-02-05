///////////////////////////////////////////////////////////////////////////////
//
/// \file       message.h
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

/// Verbosity levels
enum message_verbosity {
	V_SILENT,   ///< No messages
	V_ERROR,    ///< Only error messages
	V_WARNING,  ///< Errors and warnings
	V_VERBOSE,  ///< Errors, warnings, and verbose statistics
	V_DEBUG,    ///< Debugging, FIXME remove?
};


/// \brief      Initializes the message functions
///
/// \param      argv0       Name of the program i.e. argv[0] from main()
/// \param      verbosity   Verbosity level
///
/// If an error occurs, this function doesn't return.
///
extern void message_init(const char *argv0);


/// Increase verbosity level by one step unless it was at maximum.
extern void message_verbosity_increase(void);

/// Decrease verbosity level by one step unless it was at minimum.
extern void message_verbosity_decrease(void);


/// Set the total number of files to be processed (stdin is counted as a file
/// here). The default is one.
extern void message_set_files(unsigned int files);


/// \brief      Print a message if verbosity level is at least "verbosity"
///
/// This doesn't touch the exit status.
extern void message(enum message_verbosity verbosity, const char *fmt, ...)
		lzma_attribute((format(printf, 2, 3)));


/// \brief      Prints a warning and possibly sets exit status
///
/// The message is printed only if verbosity level is at least V_WARNING.
/// The exit status is set to WARNING unless it was already at ERROR.
extern void message_warning(const char *fmt, ...)
		lzma_attribute((format(printf, 1, 2)));


/// \brief      Prints an error message and sets exit status
///
/// The message is printed only if verbosity level is at least V_ERROR.
/// The exit status is set to ERROR.
extern void message_error(const char *fmt, ...)
		lzma_attribute((format(printf, 1, 2)));


/// \brief      Prints an error message and exits with EXIT_ERROR
///
/// The message is printed only if verbosity level is at least V_ERROR.
extern void message_fatal(const char *fmt, ...)
		lzma_attribute((format(printf, 1, 2)))
		lzma_attribute((noreturn));


/// Print an error message that an internal error occurred and exit with
/// EXIT_ERROR.
extern void message_bug(void) lzma_attribute((noreturn));


/// Print a message that establishing signal handlers failed, and exit with
/// exit status ERROR.
extern void message_signal_handler(void) lzma_attribute((noreturn));


/// Convert lzma_ret to a string.
extern const char *message_strm(lzma_ret code);


/// Print the filter chain.
extern void message_filters(
		enum message_verbosity v, const lzma_filter *filters);


/// Print a message that user should try --help.
extern void message_try_help(void);


/// Prints the version number to stdout and exits with exit status SUCCESS.
extern void message_version(void) lzma_attribute((noreturn));


/// Print the help message.
extern void message_help(bool long_help) lzma_attribute((noreturn));


///
extern void message_progress_start(const char *filename, uint64_t in_size);


///
extern void message_progress_update(uint64_t in_pos, uint64_t out_pos);


/// \brief      Finishes the progress message if we were in verbose mode
///
/// \param      in_pos      Final input position i.e. how much input there was.
/// \param      out_pos     Final output position
/// \param      success     True if the operation was successful. We don't
///                         print the final progress message if the operation
///                         wasn't successful.
///
extern void message_progress_end(
		uint64_t in_pos, uint64_t out_pos, bool success);
