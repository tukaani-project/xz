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

noinst_LTLIBRARIES = liblz.la
liblz_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common \
	-I@top_srcdir@/src/liblzma/check
liblz_la_SOURCES =


if COND_ENCODER_LZ
liblz_la_SOURCES += \
	lz_encoder.c \
	lz_encoder.h \
	lz_encoder_hash.h \
	lz_encoder_mf.c
endif


if COND_DECODER_LZ
liblz_la_SOURCES += \
	lz_decoder.c \
	lz_decoder.h
endif
