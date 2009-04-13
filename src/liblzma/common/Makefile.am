##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
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
	easy_preset.c \
	easy_preset.h \
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
	easy_buffer_encoder.c \
	easy_encoder.c \
	easy_encoder_memusage.c \
	filter_buffer_encoder.c \
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
	block_buffer_decoder.c \
	block_decoder.c \
	block_decoder.h \
	block_header_decoder.c \
	easy_decoder_memusage.c \
	filter_buffer_decoder.c \
	filter_decoder.c \
	filter_decoder.h \
	filter_flags_decoder.c \
	index_decoder.c \
	index_hash.c \
	stream_buffer_decoder.c \
	stream_decoder.c \
	stream_decoder.h \
	stream_flags_decoder.c \
	vli_decoder.c
endif
