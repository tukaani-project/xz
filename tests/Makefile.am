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

EXTRA_DIST = \
	files \
	tests.h \
	test_files.sh \
	test_compress.sh \
	bcj_test.c \
	compress_prepared_bcj_sparc \
	compress_prepared_bcj_x86

AM_CPPFLAGS = \
	-I@top_srcdir@/src/common \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_builddir@/lib

LDADD = @top_builddir@/src/liblzma/liblzma.la

if COND_GNULIB
LDADD += @top_builddir@/lib/libgnu.a
endif

check_PROGRAMS = \
	create_compress_files \
	test_memlimit \
	test_check \
	test_stream_flags \
	test_filter_flags \
	test_block_header \
	test_index

TESTS = \
	test_memlimit \
	test_check \
	test_stream_flags \
	test_filter_flags \
	test_block_header \
	test_index \
	test_compress.sh

clean-local:
	-rm -f compress_generated_*
