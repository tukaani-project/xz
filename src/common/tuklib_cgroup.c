// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_cgroup.c
/// \brief      Read Linux cgroup v2 limits
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "tuklib_cgroup.h"

#ifndef __linux__

// Prevent an empty translation unit.
typedef int dummy;

#else

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define SYS_FS_CGROUP "/sys/fs/cgroup"
#define SYS_FS_CGROUP_LEN (sizeof(SYS_FS_CGROUP) - 1)


static size_t
get_cgroup_path(char buf[PATH_MAX])
{
	// Find the pathname of the control group of this process and store it
	// in buf with a trailing slash. Only cgroup v2 is supported.
	//
	// Syscall counts with glibc 2.43:
	//     fopen + fread + fclose = 5: openat + fstat + read + read + close
	//     fopen + fgets + fclose = 4: openat + fstat + read + close
	//     open + read + close = 3:    openat + read + close
	//
	// Use open + read + close to minimize the number of syscalls.
	// Partial reads or EINTR shouldn't occur with proc or cgroup2 fs.
	int fd = open("/proc/self/cgroup", O_RDONLY);
	if (fd == -1)
		return 0;

	// Read the data from fd into an offset so that we can insert
	// the "/sys/fs/cgroup" prefix. Reserve one byte of space so
	// that we can append \0. The \n may be overwritten with "/".
	//     buf after read(): "...........0::/foo/bar\n"  (without \0)
	//     final buf:        "/sys/fs/cgroup/foo/bar/\0"
	const size_t offset = SYS_FS_CGROUP_LEN - 3; // 3 == strlen("0::")
	ssize_t read_size = read(fd, buf + offset, PATH_MAX - offset - 1);
	(void)close(fd);

	// With cgroup v2, the file should begin with "0::/" (4 bytes)
	// and there should be "\n" (1 byte) that terminates the line.
	if (read_size < 4 + 1 || memcmp(buf + offset, "0::/", 4) != 0)
		return 0;

	// Add the "/sys/fs/cgroup" prefix, overwriting "0::".
	memcpy(buf, SYS_FS_CGROUP, SYS_FS_CGROUP_LEN);

	// Terminate the string so that we can use strcspn().
	// We reserved space for this in the read() call.
	buf[offset + (size_t)read_size] = '\0';

	// Find the length of the line. If \n isn't found, either the line
	// didn't fit into buf or there is an unexpected \0 before \n.
	size_t len = strcspn(buf, "\n");
	if (buf[len] != '\n')
		return 0;

	// Replace the \n with a slash except if the previous char is a slash.
	// Double-slash would make the calling function process the same
	// directory twice.
	if (buf[len - 1] != '/')
		buf[len++] = '/';

	// Terminate the line. We reserved space for this in the read() call.
	buf[len] = '\0';

	// Reject the pathname if there might be a path traversal problem.
	// This shouldn't happen with /proc/self/cgroup, but check it anyway.
	if (strstr(buf, "/../") != NULL)
		return 0;

	return len;
}


static unsigned long long
get_cgroup_limit(const char *filename,
		unsigned long long (*parse)(const char *str))
{
	// Get the control group pathname and its length. The path will
	// have a trailing slash after which the filename can be appended.
	char buf[PATH_MAX];
	size_t len = get_cgroup_path(buf);
	if (len == 0)
		return ULLONG_MAX;

	// Check that there is space to append the filename, including the \0.
	const size_t filename_size = strlen(filename) + 1;
	if (len + filename_size > sizeof(buf))
		return ULLONG_MAX;

	// Read the file from the cgroup and from its parent cgroups. Set ret
	// to the lowest value found or ULLONG_MAX if there are no limits.
	unsigned long long ret = ULLONG_MAX;
	while (len > SYS_FS_CGROUP_LEN) {
		// Append the filename (including \0) after
		// the directory separator (slash) in buf.
		memcpy(buf + len, filename, filename_size);

		// The file should exist in all non-root cgroups.
		// If opening it fails, don't waste time looking at
		// parent directories.
		const int fd = open(buf, O_RDONLY);
		if (fd == -1)
			break;

		// The file should contain one short line.
		char line[64];
		const ssize_t read_size = read(fd, line, sizeof(line));
		(void)close(fd);

		if (read_size > 0 && line[read_size - 1] == '\n') {
			line[read_size - 1] = '\0';
			const unsigned long long value = parse(line);
			if (ret > value)
				ret = value;
		}

		// Continue from the parent directory.
		while (buf[--len - 1] != '/') ;
	}

	return ret;
}


static unsigned long long
parse_memory_max(const char *str)
{
	// memory.max should contain either an unsigned base-10 integer
	// or "max". Zero isn't a sensible memory.max value, and it's
	// fine if values larger than ULLONG_MAX become ULLONG_MAX.
	// This allows simplifying strtoull() error checking.
	char *end;
	const unsigned long long value = strtoull(str, &end, 10);
	return value > 0 && *end == '\0' ? value : ULLONG_MAX;
}


static unsigned long long
parse_cpu_max(const char *str)
{
	// cpu.max should contain a space-separated pair "MAX PERIOD"
	// where MAX is either "max" or an unsigned base-10 integer,
	// and PERIOD is an unsigned base-10 integer. Zeros aren't
	// sensible values in cpu.max.
	//
	// Return the number of threads that can be utilized at 100 %,
	// but don't return 0 (return at least 1).
	unsigned long long ret = ULLONG_MAX;
	char *end;
	const unsigned long long max = strtoull(str, &end, 10);
	if (max > 0 && *end == ' ') {
		const unsigned long long period = strtoull(end, &end, 10);
		if (period > 0 && *end == '\0')
			ret = max > period ? max / period : 1;
	}

	return ret;
}


extern unsigned long long
tuklib_cgroup_memory_max(void)
{
	return get_cgroup_limit("memory.max", &parse_memory_max);
}


extern unsigned long long
tuklib_cgroup_cpu_max(void)
{
	return get_cgroup_limit("cpu.max", &parse_cpu_max);
}

#endif
