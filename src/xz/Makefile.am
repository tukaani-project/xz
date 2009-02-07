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

bin_PROGRAMS = xz

xz_SOURCES = \
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
	signals.c \
	signals.h \
	suffix.c \
	suffix.h \
	util.c \
	util.h

xz_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I@top_srcdir@/src/common \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_builddir@/lib \
	-I@top_srcdir@/lib \
	@STATIC_CPPFLAGS@

xz_LDFLAGS = @STATIC_LDFLAGS@
xz_LDADD = \
	@top_builddir@/src/liblzma/liblzma.la \
	@LTLIBINTL@

if COND_GNULIB
xz_LDADD += @top_builddir@/lib/libgnu.a
endif


## Create symlinks for unxz and xzcat for convenicen. Create symlinks also
## for lzma, unlzma, and lzcat for compatibility with LZMA Utils 4.32.x.
install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f unxz xzcat lzma unlzma lzcat && \
	$(LN_S) xz unxz && \
	$(LN_S) xz xzcat && \
	$(LN_S) xz lzma && \
	$(LN_S) xz unlzma && \
	$(LN_S) xz lzcat

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f unxz xzcat lzma unlzma lzcat
