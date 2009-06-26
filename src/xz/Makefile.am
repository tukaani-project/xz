##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
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
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	$(STATIC_CPPFLAGS)

xz_LDFLAGS = $(STATIC_LDFLAGS)
xz_LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
xz_LDADD += $(top_builddir)/lib/libgnu.a
endif

# libgnu.a may need these libs, so this must be after libgnu.a.
xz_LDADD += $(LTLIBINTL)


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
