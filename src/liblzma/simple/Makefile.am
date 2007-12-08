##
##  Copyright (C) 2007 Lasse Collin
##
##  This library is free software; you can redistribute it and/or
##  modify it under the terms of the GNU Lesser General Public
##  License as published by the Free Software Foundation; either
##  version 2.1 of the License, or (at your option) any later version.
##
##  This library is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##  Lesser General Public License for more details.
##

noinst_LTLIBRARIES = libsimple.la
libsimple_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common
libsimple_la_SOURCES = \
	simple_coder.c \
	simple_coder.h \
	simple_private.h

if COND_FILTER_X86
libsimple_la_SOURCES += x86.c
endif

if COND_FILTER_POWERPC
libsimple_la_SOURCES += powerpc.c
endif

if COND_FILTER_IA64
libsimple_la_SOURCES += ia64.c
endif

if COND_FILTER_ARM
libsimple_la_SOURCES += arm.c
endif

if COND_FILTER_ARMTHUMB
libsimple_la_SOURCES += armthumb.c
endif

if COND_FILTER_SPARC
libsimple_la_SOURCES += sparc.c
endif
