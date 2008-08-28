##
##  Copyright (C) 2008 Lasse Collin
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

noinst_LTLIBRARIES = libdelta.la
libdelta_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common

libdelta_la_SOURCES = \
	delta_common.c \
	delta_common.h

if COND_ENCODER_DELTA
libdelta_la_SOURCES += \
	delta_encoder.c \
	delta_encoder.h
endif

if COND_DECODER_DELTA
libdelta_la_SOURCES += \
	delta_decoder.c \
	delta_decoder.h
endif
