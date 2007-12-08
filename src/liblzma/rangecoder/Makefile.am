##
##  Copyright (C) 2006 Lasse Collin
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

noinst_LTLIBRARIES = librangecoder.la

librangecoder_la_SOURCES = range_common.h
librangecoder_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common

if COND_MAIN_ENCODER
librangecoder_la_SOURCES += range_encoder.c range_encoder.h
endif

if COND_MAIN_DECODER
librangecoder_la_SOURCES += range_decoder.h
endif
