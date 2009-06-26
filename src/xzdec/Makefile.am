##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

bin_PROGRAMS = xzdec lzmadec

xzdec_SOURCES = xzdec.c
xzdec_CPPFLAGS = \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_builddir)/lib \
	$(STATIC_CPPFLAGS)
xzdec_LDFLAGS = $(STATIC_LDFLAGS)
xzdec_LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
xzdec_LDADD += $(top_builddir)/lib/libgnu.a
endif

xzdec_LDADD += $(LTLIBINTL)


lzmadec_SOURCES = $(xzdec_SOURCES)
lzmadec_CPPFLAGS = $(xzdec_CPPFLAGS) -DLZMADEC
lzmadec_LDFLAGS = $(xzdec_LDFLAGS)
lzmadec_LDADD = $(xzdec_LDADD)

dist_man_MANS = xzdec.1

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f lzmadec.1 && \
	$(LN_S) xzdec.1 lzmadec.1

uninstall-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	rm -f lzmadec.1
