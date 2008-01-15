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
	sysdefs.h \
	bsr.h \
	allocator.c \
	block_private.h \
	extra.c \
	features.c \
	index.c \
	info.c \
	init.c \
	memory_limitter.c \
	memory_usage.c \
	next_coder.c \
	raw_common.c \
	raw_common.h \
	code.c \
	version.c

if COND_FILTER_COPY
libcommon_la_SOURCES += \
	copy_coder.c \
	copy_coder.h
endif

if COND_FILTER_DELTA
libcommon_la_SOURCES += \
	delta_coder.c \
	delta_coder.h
endif

if COND_MAIN_ENCODER
libcommon_la_SOURCES += \
	alignment.c \
	auto_decoder.c \
	alone_encoder.c \
	block_encoder.c \
	block_encoder.h \
	block_header_encoder.c \
	filter_flags_encoder.c \
	init_encoder.c \
	metadata_encoder.c \
	metadata_encoder.h \
	raw_encoder.c \
	raw_encoder.h \
	stream_common.c \
	stream_common.h \
	stream_encoder_single.c \
	stream_encoder_multi.c \
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
	init_decoder.c \
	metadata_decoder.c \
	metadata_decoder.h \
	raw_decoder.c \
	raw_decoder.h \
	stream_decoder.c \
	stream_flags_decoder.c \
	stream_flags_decoder.h \
	vli_decoder.c \
	vli_reverse_decoder.c
endif
