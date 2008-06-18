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
	-I@top_srcdir@/src/liblzma/lz \
	-I@top_srcdir@/src/liblzma/lzma \
	-I@top_srcdir@/src/liblzma/simple \
	-I@top_srcdir@/src/liblzma/subblock \
	-I@top_srcdir@/src/liblzma/rangecoder
libcommon_la_SOURCES = \
	common.h \
	bsr.h \
	allocator.c \
	block_util.c \
	block_private.h \
	features.c \
	index.c \
	init.c \
	memory_limiter.c \
	memory_usage.c \
	next_coder.c \
	raw_common.c \
	raw_common.h \
	stream_flags_equal.c \
	code.c \
	version.c

if COND_FILTER_DELTA
libcommon_la_SOURCES += \
	delta_common.c \
	delta_common.h
if COND_MAIN_ENCODER
libcommon_la_SOURCES += \
	delta_encoder.c \
	delta_encoder.h
endif
if COND_MAIN_DECODER
libcommon_la_SOURCES += \
	delta_decoder.c \
	delta_decoder.h
endif
endif

if COND_MAIN_ENCODER
libcommon_la_SOURCES += \
	alignment.c \
	auto_decoder.c \
	alone_encoder.c \
	block_encoder.c \
	block_encoder.h \
	block_header_encoder.c \
	easy.c \
	filter_flags_encoder.c \
	index_encoder.c \
	index_encoder.h \
	init_encoder.c \
	raw_encoder.c \
	raw_encoder.h \
	stream_common.c \
	stream_common.h \
	stream_encoder.c \
	stream_encoder.h \
	stream_flags_encoder.c \
	vli_encoder.c
endif

if COND_MAIN_DECODER
libcommon_la_SOURCES += \
	alone_decoder.c \
	alone_decoder.h \
	block_decoder.c \
	block_decoder.h \
	block_header_decoder.c \
	filter_flags_decoder.c \
	index_decoder.c \
	index_hash.c \
	init_decoder.c \
	raw_decoder.c \
	raw_decoder.h \
	stream_decoder.c \
	stream_flags_decoder.c \
	stream_flags_decoder.h \
	vli_decoder.c
endif
