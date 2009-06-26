##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

EXTRA_DIST = price_tablegen.c

noinst_LTLIBRARIES = librangecoder.la

librangecoder_la_SOURCES = range_common.h
librangecoder_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common

if COND_ENCODER_LZMA1
librangecoder_la_SOURCES += \
	range_encoder.h \
	price.h \
	price_table.c
endif

if COND_DECODER_LZMA1
librangecoder_la_SOURCES += range_decoder.h
endif
