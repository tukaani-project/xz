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
	coder.c \
	coder.h \
	file_io.c \
	file_io.h \
	hardware.c \
	hardware.h \
	main.c \
	main.h \
	message.c \
	message.h \
	options.c \
	options.h \
	private.h \
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


## Create symlinks for unxz and xzcat for convenience. Create symlinks also
## for lzma, unlzma, and lzcat for compatibility with LZMA Utils 4.32.x.
xzlinks = unxz xzcat lzma unlzma lzcat

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	target=`echo xz | $(SED) '$(transform)'` && \
	for name in $(xzlinks); do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	for name in $(xzlinks); do \
		link=`echo $$name | $(SED) '$(transform)'` && \
		rm -f $$link; \
	done
