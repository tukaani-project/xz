##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

noinst_LTLIBRARIES = libdelta.la
libdelta_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common

libdelta_la_SOURCES = \
	delta_common.c \
	delta_common.h \
	delta_private.h

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
