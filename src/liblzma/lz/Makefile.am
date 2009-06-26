##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

noinst_LTLIBRARIES = liblz.la
liblz_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common \
	-I$(top_srcdir)/src/liblzma/check
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
