///////////////////////////////////////////////////////////////////////////////
//
/// \file       open_stdxxx.h
/// \brief      Make sure that file descriptors 0, 1, and 2 are open
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef OPEN_STDXXX_H
#define OPEN_STDXXX_H

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


static void
open_stdxxx(int status)
{
	for (int i = 0; i <= 2; ++i) {
		// We use fcntl() to check if the file descriptor is open.
		if (fcntl(i, F_GETFD) == -1 && errno == EBADF) {
			// With stdin, we could use /dev/full so that
			// writing to stdin would fail. However, /dev/full
			// is Linux specific, and if the program tries to
			// write to stdin, there's already a problem anyway.
			const int fd = open("/dev/null", O_NOCTTY
					| (i == 0 ? O_WRONLY : O_RDONLY));

			if (fd != i) {
				// Something went wrong. Exit with the
				// exit status we were given. Don't try
				// to print an error message, since stderr
				// may very well be non-existent. This
				// error should be extremely rare.
				(void)close(fd);
				exit(status);
			}
		}
	}

	return;
}

#endif
