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


if COND_MAIN_ENCODER
liblz_la_SOURCES += \
	lz_encoder.c \
	lz_encoder.h \
	lz_encoder_private.h \
	match_c.h \
	match_h.h

if COND_MF_HC3
liblz_la_SOURCES += hc3.c hc3.h
liblz_la_CPPFLAGS += -DHAVE_HC3
endif

if COND_MF_HC4
liblz_la_SOURCES += hc4.c hc4.h
liblz_la_CPPFLAGS += -DHAVE_HC4
endif

if COND_MF_BT2
liblz_la_SOURCES += bt2.c bt2.h
liblz_la_CPPFLAGS += -DHAVE_BT2
endif

if COND_MF_BT3
liblz_la_SOURCES += bt3.c bt3.h
liblz_la_CPPFLAGS += -DHAVE_BT3
endif

if COND_MF_BT4
liblz_la_SOURCES += bt4.c bt4.h
liblz_la_CPPFLAGS += -DHAVE_BT4
endif

endif


if COND_MAIN_DECODER
liblz_la_SOURCES += \
	lz_decoder.c \
	lz_decoder.h
endif
