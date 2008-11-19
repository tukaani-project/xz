##
##  Copyright (C) 2007 Lasse Collin
##
##  This program is free software; you can redistribute it and/or
##  modify it under the terms of the GNU Lesser General Public
##  License as published by the Free Software Foundation; either
##  version 2.1 of the License, or (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##  Lesser General Public License for more details.
##

bin_PROGRAMS = lzma

lzma_SOURCES = \
	args.c \
	args.h \
	hardware.c \
	hardware.h \
	io.c \
	io.h \
	main.c \
	main.h \
	message.c \
	message.h \
	options.c \
	options.h \
	private.h \
	process.c \
	process.h \
	suffix.c \
	suffix.h \
	util.c \
	util.h

## It must be able to find sysdefs.h, lzma_adv.h, and possible
## replacement headers.
lzma_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I@top_srcdir@/src/common \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_builddir@/lib \
	-I@top_srcdir@/lib

lzma_CFLAGS = @PTHREAD_CFLAGS@

## Always link the command line tool statically against liblzma. It is
## faster on x86, because no need for PIC. We also have one dependency less,
## which allows users to more freely copy the lzma binary to other boxes.
lzma_LDFLAGS = -static
lzma_LDADD = \
	@top_builddir@/src/liblzma/liblzma.la \
	@LTLIBINTL@ \
	@PTHREAD_LIBS@

if COND_GNULIB
lzma_LDADD += @top_builddir@/lib/libgnu.a
endif


## Create symlinks for unlzma and lzcat:
install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f unlzma lzcat && \
	$(LN_S) lzma unlzma && \
	$(LN_S) lzma lzcat

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f unlzma lzcat
