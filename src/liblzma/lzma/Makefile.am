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

EXTRA_DIST = fastpos_tablegen.c

## Using liblzma2 since liblzma is already used for the final library.
noinst_LTLIBRARIES = liblzma2.la
liblzma2_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common \
	-I@top_srcdir@/src/liblzma/lz \
	-I@top_srcdir@/src/liblzma/rangecoder

liblzma2_la_SOURCES = lzma_common.h

if COND_ENCODER_LZMA1
liblzma2_la_SOURCES += \
	fastpos.h \
	lzma_encoder.h \
	lzma_encoder.c \
	lzma_encoder_presets.c \
	lzma_encoder_private.h \
	lzma_encoder_optimum_fast.c \
	lzma_encoder_optimum_normal.c

if !COND_SMALL
liblzma2_la_SOURCES += fastpos_table.c
endif
endif

if COND_DECODER_LZMA1
liblzma2_la_SOURCES += \
	lzma_decoder.c \
	lzma_decoder.h
endif

if COND_ENCODER_LZMA2
liblzma2_la_SOURCES += \
	lzma2_encoder.c \
	lzma2_encoder.h
endif

if COND_DECODER_LZMA2
liblzma2_la_SOURCES += \
	lzma2_decoder.c \
	lzma2_decoder.h
endif
