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

SUBDIRS = api common check

lib_LTLIBRARIES = liblzma.la
liblzma_la_SOURCES =
liblzma_la_LDFLAGS = -version-info 0:0:0

liblzma_la_LIBADD = \
	common/libcommon.la \
	check/libcheck.la

if COND_FILTER_LZMA
SUBDIRS += lz lzma rangecoder
liblzma_la_LIBADD += \
	lz/liblz.la \
	lzma/liblzma4.la \
	rangecoder/librangecoder.la
endif

if COND_FILTER_SUBBLOCK
SUBDIRS += subblock
liblzma_la_LIBADD += subblock/libsubblock.la
endif

if COND_MAIN_SIMPLE
SUBDIRS += simple
liblzma_la_LIBADD += simple/libsimple.la
endif


## pkg-config
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lzma.pc
EXTRA_DIST = lzma.pc.in
