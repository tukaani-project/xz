##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

EXTRA_DIST = fastpos_tablegen.c

## Using liblzma2 since liblzma is already used for the final library.
noinst_LTLIBRARIES = liblzma2.la
liblzma2_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common \
	-I$(top_srcdir)/src/liblzma/lz \
	-I$(top_srcdir)/src/liblzma/rangecoder

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
