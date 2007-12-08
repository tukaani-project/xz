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

noinst_LTLIBRARIES = libsubblock.la
libsubblock_la_SOURCES =
libsubblock_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common

if COND_MAIN_ENCODER
libsubblock_la_SOURCES += \
	subblock_encoder.c \
	subblock_encoder.h
endif

if COND_MAIN_DECODER
libsubblock_la_SOURCES += \
	subblock_decoder.c \
	subblock_decoder.h \
	subblock_decoder_helper.c \
	subblock_decoder_helper.h
endif
