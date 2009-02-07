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

bin_PROGRAMS = xzdec lzmadec

xzdec_SOURCES = xzdec.c
xzdec_CPPFLAGS = \
	-I@top_srcdir@/src/common \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_builddir@/lib \
	@STATIC_CPPFLAGS@
xzdec_LDFLAGS = @STATIC_LDFLAGS@
xzdec_LDADD = \
	@top_builddir@/src/liblzma/liblzma.la \
	@LTLIBINTL@

if COND_GNULIB
xzdec_LDADD += @top_builddir@/lib/libgnu.a
endif

lzmadec_SOURCES = $(xzdec_SOURCES)
lzmadec_CPPFLAGS = $(xzdec_CPPFLAGS) -DLZMADEC
lzmadec_LDFLAGS = $(xzdec_LDFLAGS)
lzmadec_LDADD = $(xzdec_LDADD)
