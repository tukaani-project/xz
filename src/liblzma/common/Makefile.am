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

noinst_LTLIBRARIES = libcommon.la
libcommon_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/check \
	-I@top_srcdir@/src/liblzma/rangecoder \
	-I@top_srcdir@/src/liblzma/lz \
	-I@top_srcdir@/src/liblzma/lzma \
	-I@top_srcdir@/src/liblzma/subblock \
	-I@top_srcdir@/src/liblzma/delta \
	-I@top_srcdir@/src/liblzma/simple

libcommon_la_SOURCES = \
	common.c \
	common.h \
	bsr.h \
	block_util.c \
	filter_common.c \
	filter_common.h \
	index.c \
	index.h \
	stream_flags_common.c \
	stream_flags_common.h \
	vli_size.c

if COND_MAIN_ENCODER
libcommon_la_SOURCES += \
	alone_encoder.c \
	block_buffer_encoder.c \
	block_encoder.c \
	block_encoder.h \
	block_header_encoder.c \
	easy.c \
	filter_encoder.c \
	filter_encoder.h \
	filter_flags_encoder.c \
	index_encoder.c \
	index_encoder.h \
	stream_buffer_encoder.c \
	stream_encoder.c \
	stream_encoder.h \
	stream_flags_encoder.c \
	vli_encoder.c
endif

if COND_MAIN_DECODER
libcommon_la_SOURCES += \
	alone_decoder.c \
	alone_decoder.h \
	auto_decoder.c \
	block_decoder.c \
	block_decoder.h \
	block_header_decoder.c \
	filter_decoder.c \
	filter_decoder.h \
	filter_flags_decoder.c \
	index_decoder.c \
	index_hash.c \
	stream_decoder.c \
	stream_decoder.h \
	stream_flags_decoder.c \
	vli_decoder.c
endif
