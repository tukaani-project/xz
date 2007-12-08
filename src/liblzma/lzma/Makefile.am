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

noinst_LTLIBRARIES = liblzma4.la
liblzma4_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common \
	-I@top_srcdir@/src/liblzma/lz \
	-I@top_srcdir@/src/liblzma/rangecoder

liblzma4_la_SOURCES = \
	lzma_common.h \
	lzma_literal.c \
	lzma_literal.h

if COND_MAIN_ENCODER
liblzma4_la_SOURCES += \
	lzma_encoder.h \
	lzma_encoder.c \
	lzma_encoder_presets.c \
	lzma_encoder_private.h \
	lzma_encoder_init.c \
	lzma_encoder_features.c \
	lzma_encoder_getoptimum.c \
	lzma_encoder_getoptimumfast.c
endif

if COND_MAIN_DECODER
liblzma4_la_SOURCES += \
	lzma_decoder.c \
	lzma_decoder.h
endif
