##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

noinst_LTLIBRARIES = libsubblock.la
libsubblock_la_SOURCES =
libsubblock_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common

if COND_ENCODER_SUBBLOCK
libsubblock_la_SOURCES += \
	subblock_encoder.c \
	subblock_encoder.h
endif

if COND_DECODER_SUBBLOCK
libsubblock_la_SOURCES += \
	subblock_decoder.c \
	subblock_decoder.h \
	subblock_decoder_helper.c \
	subblock_decoder_helper.h
endif
