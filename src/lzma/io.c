///////////////////////////////////////////////////////////////////////////////
//
/// \file       io.c
/// \brief      File opening, unlinking, and closing
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

#if defined(HAVE_FUTIMES) || defined(HAVE_FUTIMESAT)
#	include <sys/time.h>
#endif

#ifndef O_SEARCH
#	define O_SEARCH O_RDONLY
#endif


/// \brief      Number of open file_pairs
///
/// Once the main() function has requested processing of all files,
/// we wait that open_pairs drops back to zero. Then it is safe to
/// exit from the program.
static size_t open_pairs = 0;


/// \brief      mutex for file system operations
///
/// All file system operations are done via the functions in this file.
/// They use fchdir() to avoid some race conditions (more portable than
/// openat() & co.).
///
/// Synchronizing all file system operations shouldn't affect speed notably,
/// since the actual reading from and writing to files is done in parallel.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/// This condition is invoked when a file is closed and the value of
/// the open_files variable has dropped to zero. The only listener for
/// this condition is io_finish() which is called from main().
static pthread_cond_t io_cond = PTHREAD_COND_INITIALIZER;


/// True when stdout is being used by some thread
static bool stdout_in_use = false;


/// This condition is signalled when a thread releases stdout (no longer
/// writes data to it).
static pthread_cond_t stdout_cond = PTHREAD_COND_INITIALIZER;


/// \brief      Directory where we were started
///
/// This is needed when a new file, whose name was given on command line,
/// is opened.
static int start_dir;


static uid_t uid;
static gid_t gid;


extern void
io_init(void)
{
	start_dir = open(".", O_SEARCH | O_NOCTTY);
	if (start_dir == -1) {
		errmsg(V_ERROR, _("Cannot get file descriptor of the current "
				"directory: %s"), strerror(errno));
		my_exit(ERROR);
	}

	uid = getuid();
	gid = getgid();

	return;
}


/// Waits until the number of open file_pairs has dropped to zero.
extern void
io_finish(void)
{
	pthread_mutex_lock(&mutex);

	while (open_pairs != 0)
		pthread_cond_wait(&io_cond, &mutex);

	(void)close(start_dir);

	pthread_mutex_unlock(&mutex);

	return;
}


/// \brief      Unlinks a file
///
/// \param      dir_fd  File descriptor of the directory containing the file
/// \param      name    Name of the file with or without path
///
/// \return     Zero on success. On error, -1 is returned and errno set.
///
static void
io_unlink(int dir_fd, const char *name, ino_t ino)
{
	const char *base = str_filename(name);
	if (base == NULL) {
		// This shouldn't happen.
		errmsg(V_ERROR, _("%s: Invalid filename"), name);
		return;
	}

	pthread_mutex_lock(&mutex);

	if (fchdir(dir_fd)) {
		errmsg(V_ERROR, _("Cannot change directory: %s"),
				strerror(errno));
	} else {
		struct stat st;
		if (lstat(base, &st) || st.st_ino != ino)
			errmsg(V_ERROR, _("%s: File seems to be moved, "
					"not removing"), name);

		// There's a race condition between lstat() and unlink()
		// but at least we have tried to avoid removing wrong file.
		else if (unlink(base))
			errmsg(V_ERROR, _("%s: Cannot remove: %s"),
					name, strerror(errno));
	}

	pthread_mutex_unlock(&mutex);

	return;
}


/// \brief      Copies owner/group and permissions
///
/// \todo       ACL and EA support
///
static void
io_copy_attrs(const file_pair *pair)
{
	// This function is more tricky than you may think at first.
	// Blindly copying permissions may permit users to access the
	// destination file who didn't have permission to access the
	// source file.

	if (uid == 0 && fchown(pair->dest_fd, pair->src_st.st_uid, -1))
		errmsg(V_WARNING, _("%s: Cannot set the file owner: %s"),
				pair->dest_name, strerror(errno));

	mode_t mode;

	if (fchown(pair->dest_fd, -1, pair->src_st.st_gid)) {
		errmsg(V_WARNING, _("%s: Cannot set the file group: %s"),
				pair->dest_name, strerror(errno));
		// We can still safely copy some additional permissions:
		// `group' must be at least as strict as `other' and
		// also vice versa.
		//
		// NOTE: After this, the owner of the source file may
		// get additional permissions. This shouldn't be too bad,
		// because the owner would have had permission to chmod
		// the original file anyway.
		mode = ((pair->src_st.st_mode & 0070) >> 3)
				& (pair->src_st.st_mode & 0007);
		mode = (pair->src_st.st_mode & 0700) | (mode << 3) | mode;
	} else {
		// Drop the setuid, setgid, and sticky bits.
		mode = pair->src_st.st_mode & 0777;
	}

	if (fchmod(pair->dest_fd, mode))
		errmsg(V_WARNING, _("%s: Cannot set the file permissions: %s"),
				pair->dest_name, strerror(errno));

	// Copy the timestamps only if we have a secure function to do it.
#if defined(HAVE_FUTIMES) || defined(HAVE_FUTIMESAT)
	struct timeval tv[2];
	tv[0].tv_sec = pair->src_st.st_atime;
	tv[1].tv_sec = pair->src_st.st_mtime;

#	if defined(HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
	tv[0].tv_usec = pair->src_st.st_atim.tv_nsec / 1000;
#	elif defined(HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC)
	tv[0].tv_usec = pair->src_st.st_atimespec.tv_nsec / 1000;
#	else
	tv[0].tv_usec = 0;
#	endif

#	if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	tv[1].tv_usec = pair->src_st.st_mtim.tv_nsec / 1000;
#	elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
	tv[1].tv_usec = pair->src_st.st_mtimespec.tv_nsec / 1000;
#	else
	tv[1].tv_usec = 0;
#	endif

#	ifdef HAVE_FUTIMES
	(void)futimes(pair->dest_fd, tv);
#	else
	(void)futimesat(pair->dest_fd, NULL, tv);
#	endif
#endif

	return;
}


/// Opens and changes into the directory containing the source file.
static int
io_open_dir(file_pair *pair)
{
	if (pair->src_name == stdin_filename)
		return 0;

	if (fchdir(start_dir)) {
		errmsg(V_ERROR, _("Cannot change directory: %s"),
				strerror(errno));
		return -1;
	}

	const char *split = strrchr(pair->src_name, '/');
	if (split == NULL) {
		pair->dir_fd = start_dir;
	} else {
		// Copy also the slash. It's needed to support filenames
		// like "/foo" (dirname being "/"), and it never hurts anyway.
		const size_t dirname_len = split - pair->src_name + 1;
		char dirname[dirname_len + 1];
		memcpy(dirname, pair->src_name, dirname_len);
		dirname[dirname_len] = '\0';

		// Open the directory and change into it.
		pair->dir_fd = open(dirname, O_SEARCH | O_NOCTTY);
		if (pair->dir_fd == -1 || fchdir(pair->dir_fd)) {
			errmsg(V_ERROR, _("%s: Cannot open the directory "
					"containing the file: %s"),
					pair->src_name, strerror(errno));
			(void)close(pair->dir_fd);
			return -1;
		}
	}

	return 0;
}


static void
io_close_dir(file_pair *pair)
{
	if (pair->dir_fd != start_dir)
		(void)close(pair->dir_fd);

	return;
}


/// Opens the source file. The file is opened using the plain filename without
/// path, thus the file must be in the current working directory. This is
/// ensured because io_open_dir() is always called before this function.
static int
io_open_src(file_pair *pair)
{
	if (pair->src_name == stdin_filename) {
		pair->src_fd = STDIN_FILENO;
	} else {
		// Strip the pathname. Thanks to io_open_dir(), the file
		// is now in the current working directory.
		const char *filename = str_filename(pair->src_name);
		if (filename == NULL)
			return -1;

		// Symlinks are followed if --stdout or --force has been
		// specified.
		const bool follow_symlinks = opt_stdout || opt_force;
		pair->src_fd = open(filename, O_RDONLY | O_NOCTTY
				| (follow_symlinks ? 0 : O_NOFOLLOW));
		if (pair->src_fd == -1) {
			// Give an understandable error message in if reason
			// for failing was that the file was a symbolic link.
			//  - Linux, OpenBSD, Solaris: ELOOP
			//  - FreeBSD: EMLINK
			//  - Tru64: ENOTSUP
			// It seems to be safe to check for all these, since
			// those errno values aren't used for other purporses
			// on any of the listed operating system *when* the
			// above flags are used with open().
			if (!follow_symlinks
					&& (errno == ELOOP
#ifdef EMLINK
					|| errno == EMLINK
#endif
#ifdef ENOTSUP
					|| errno == ENOTSUP
#endif
					)) {
				errmsg(V_WARNING, _("%s: Is a symbolic link, "
						"skipping"), pair->src_name);
			} else {
				errmsg(V_ERROR, "%s: %s", pair->src_name,
						strerror(errno));
			}

			return -1;
		}

		if (fstat(pair->src_fd, &pair->src_st)) {
			errmsg(V_ERROR, "%s: %s", pair->src_name,
					strerror(errno));
			goto error;
		}

		if (S_ISDIR(pair->src_st.st_mode)) {
			errmsg(V_WARNING, _("%s: Is a directory, skipping"),
					pair->src_name);
			goto error;
		}

		if (!opt_stdout) {
			if (!opt_force && !S_ISREG(pair->src_st.st_mode)) {
				errmsg(V_WARNING, _("%s: Not a regular file, "
						"skipping"), pair->src_name);
				goto error;
			}

			if (pair->src_st.st_mode & (S_ISUID | S_ISGID)) {
				// Setuid and setgid files are rejected even
				// with --force. This is good for security
				// (hopefully) but it's a bit weird to reject
				// file when --force was given. At least this
				// matches gzip's behavior.
				errmsg(V_WARNING, _("%s: File has setuid or "
						"setgid bit set, skipping"),
						pair->src_name);
				goto error;
			}

			if (!opt_force && (pair->src_st.st_mode & S_ISVTX)) {
				errmsg(V_WARNING, _("%s: File has sticky bit "
						"set, skipping"),
						pair->src_name);
				goto error;
			}

			if (pair->src_st.st_nlink > 1) {
				errmsg(V_WARNING, _("%s: Input file has more "
						"than one hard link, "
						"skipping"), pair->src_name);
				goto error;
			}
		}
	}

	return 0;

error:
	(void)close(pair->src_fd);
	return -1;
}


/// \brief      Closes source file of the file_pair structure
///
/// \param      pair    File whose src_fd should be closed
/// \param      success If true, the file will be removed from the disk if
///                     closing succeeds and --keep hasn't been used.
static void
io_close_src(file_pair *pair, bool success)
{
	if (pair->src_fd == STDIN_FILENO || pair->src_fd == -1)
		return;

	if (close(pair->src_fd)) {
		errmsg(V_ERROR, _("%s: Closing the file failed: %s"),
				pair->src_name, strerror(errno));
	} else if (success && !opt_keep_original) {
		io_unlink(pair->dir_fd, pair->src_name, pair->src_st.st_ino);
	}

	return;
}


static int
io_open_dest(file_pair *pair)
{
	if (opt_stdout || pair->src_fd == STDIN_FILENO) {
		// We don't modify or free() this.
		pair->dest_name = (char *)"(stdout)";
		pair->dest_fd = STDOUT_FILENO;

		// Synchronize the order in which files get written to stdout.
		// Unlocking the mutex is safe, because opening the file_pair
		// can no longer fail.
		while (stdout_in_use)
			pthread_cond_wait(&stdout_cond, &mutex);

		stdout_in_use = true;

	} else {
		pair->dest_name = get_dest_name(pair->src_name);
		if (pair->dest_name == NULL)
			return -1;

		// This cannot fail, because get_dest_name() doesn't return
		// invalid names.
		const char *filename = str_filename(pair->dest_name);
		assert(filename != NULL);

		pair->dest_fd = open(filename, O_WRONLY | O_NOCTTY | O_CREAT
				| (opt_force ? O_TRUNC : O_EXCL),
				S_IRUSR | S_IWUSR);
		if (pair->dest_fd == -1) {
			errmsg(V_ERROR, "%s: %s", pair->dest_name,
					strerror(errno));
			free(pair->dest_name);
			return -1;
		}

		// If this really fails... well, we have a safe fallback.
		struct stat st;
		if (fstat(pair->dest_fd, &st))
			pair->dest_ino = 0;
		else
			pair->dest_ino = st.st_ino;
	}

	return 0;
}


/// \brief      Closes destination file of the file_pair structure
///
/// \param      pair    File whose dest_fd should be closed
/// \param      success If false, the file will be removed from the disk.
///
/// \return     Zero if closing succeeds. On error, -1 is returned and
///             error message printed.
static int
io_close_dest(file_pair *pair, bool success)
{
	if (pair->dest_fd == -1)
		return 0;

	if (pair->dest_fd == STDOUT_FILENO) {
		stdout_in_use = false;
		pthread_cond_signal(&stdout_cond);
		return 0;
	}

	if (close(pair->dest_fd)) {
		errmsg(V_ERROR, _("%s: Closing the file failed: %s"),
				pair->dest_name, strerror(errno));

		// Closing destination file failed, so we cannot trust its
		// contents. Get rid of junk:
		io_unlink(pair->dir_fd, pair->dest_name, pair->dest_ino);
		free(pair->dest_name);
		return -1;
	}

	// If the operation using this file wasn't successful, we git rid
	// of the junk file.
	if (!success)
		io_unlink(pair->dir_fd, pair->dest_name, pair->dest_ino);

	free(pair->dest_name);

	return 0;
}


extern file_pair *
io_open(const char *src_name)
{
	if (is_empty_filename(src_name))
		return NULL;

	file_pair *pair = malloc(sizeof(file_pair));
	if (pair == NULL) {
		out_of_memory();
		return NULL;
	}

	*pair = (file_pair){
		.src_name = src_name,
		.dest_name = NULL,
		.dir_fd = -1,
		.src_fd = -1,
		.dest_fd = -1,
		.src_eof = false,
	};

	pthread_mutex_lock(&mutex);

	++open_pairs;

	if (io_open_dir(pair))
		goto error_dir;

	if (io_open_src(pair))
		goto error_src;

	if (user_abort || io_open_dest(pair))
		goto error_dest;

	pthread_mutex_unlock(&mutex);

	return pair;

error_dest:
	io_close_src(pair, false);
error_src:
	io_close_dir(pair);
error_dir:
	--open_pairs;
	pthread_mutex_unlock(&mutex);
	free(pair);
	return NULL;
}


/// \brief      Closes the file descriptors and frees the structure
extern void
io_close(file_pair *pair, bool success)
{
	if (success && pair->dest_fd != STDOUT_FILENO)
		io_copy_attrs(pair);

	// Close the destination first. If it fails, we must not remove
	// the source file!
	if (!io_close_dest(pair, success)) {
		// Closing destination file succeeded. Remove the source file
		// if the operation using this file pair was successful
		// and we haven't been requested to keep the source file.
		io_close_src(pair, success);
	} else {
		// We don't care if operation using this file pair was
		// successful or not, since closing the destination file
		// failed. Don't remove the original file.
		io_close_src(pair, false);
	}

	io_close_dir(pair);

	free(pair);

	pthread_mutex_lock(&mutex);

	if (--open_pairs == 0)
		pthread_cond_signal(&io_cond);

	pthread_mutex_unlock(&mutex);

	return;
}


/// \brief      Reads from a file to a buffer
///
/// \param      pair    File pair having the sourcefile open for reading
/// \param      buf     Destination buffer to hold the read data
/// \param      size    Size of the buffer; assumed be smaller than SSIZE_MAX
///
/// \return     On success, number of bytes read is returned. On end of
///             file zero is returned and pair->src_eof set to true.
///             On error, SIZE_MAX is returned and error message printed.
///
/// \note       This does no locking, thus two threads must not read from
///             the same file. This no problem in this program.
extern size_t
io_read(file_pair *pair, uint8_t *buf, size_t size)
{
	// We use small buffers here.
	assert(size < SSIZE_MAX);

	size_t left = size;

	while (left > 0) {
		const ssize_t amount = read(pair->src_fd, buf, left);

		if (amount == 0) {
			pair->src_eof = true;
			break;
		}

		if (amount == -1) {
			if (errno == EINTR) {
				if (user_abort)
					return SIZE_MAX;

				continue;
			}

			errmsg(V_ERROR, _("%s: Read error: %s"),
					pair->src_name, strerror(errno));

			// FIXME Is this needed?
			pair->src_eof = true;

			return SIZE_MAX;
		}

		buf += (size_t)(amount);
		left -= (size_t)(amount);
	}

	return size - left;
}


/// \brief      Writes a buffer to a file
///
/// \param      pair    File pair having the destination file open for writing
/// \param      buf     Buffer containing the data to be written
/// \param      size    Size of the buffer; assumed be smaller than SSIZE_MAX
///
/// \return     On success, zero is returned. On error, -1 is returned
///             and error message printed.
///
/// \note       This does no locking, thus two threads must not write to
///             the same file. This no problem in this program.
extern int
io_write(const file_pair *pair, const uint8_t *buf, size_t size)
{
	assert(size < SSIZE_MAX);

	while (size > 0) {
		const ssize_t amount = write(pair->dest_fd, buf, size);
		if (amount == -1) {
			if (errno == EINTR) {
				if (user_abort)
					return -1;

				continue;
			}

			errmsg(V_ERROR, _("%s: Write error: %s"),
					pair->dest_name, strerror(errno));
			return -1;
		}

		buf += (size_t)(amount);
		size -= (size_t)(amount);
	}

	return 0;
}
