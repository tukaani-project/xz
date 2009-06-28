##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

SUBDIRS = api common check

lib_LTLIBRARIES = liblzma.la
liblzma_la_SOURCES =
liblzma_la_LDFLAGS = -no-undefined -version-info 0:0:0

liblzma_la_LIBADD = \
	common/libcommon.la \
	check/libcheck.la

if COND_FILTER_LZ
SUBDIRS += lz
liblzma_la_LIBADD += lz/liblz.la
endif

if COND_FILTER_LZMA1
SUBDIRS += lzma rangecoder
liblzma_la_LIBADD += \
	lzma/liblzma2.la \
	rangecoder/librangecoder.la
endif

if COND_FILTER_SUBBLOCK
SUBDIRS += subblock
liblzma_la_LIBADD += subblock/libsubblock.la
endif

if COND_FILTER_DELTA
SUBDIRS += delta
liblzma_la_LIBADD += delta/libdelta.la
endif

if COND_FILTER_SIMPLE
SUBDIRS += simple
liblzma_la_LIBADD += simple/libsimple.la
endif


## pkg-config
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = liblzma.pc
EXTRA_DIST = liblzma.pc.in
