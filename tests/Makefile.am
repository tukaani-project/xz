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

EXTRA_DIST = files tests.h

AM_CPPFLAGS = \
	-I@top_srcdir@/src/common \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_builddir@/lib

LDADD = @top_builddir@/src/liblzma/liblzma.la

if COND_GNULIB
LDADD += @top_builddir@/lib/libgnu.a
endif

check_PROGRAMS = \
	test_check \
	test_stream_flags \
	test_filter_flags \
	test_block_header \
	test_index \
	test_info

test_check_SOURCES = test_check.c
test_stream_flags_SOURCES = test_stream_flags.c
test_filter_flags_SOURCES = test_filter_flags.c
test_block_header_SOURCES = test_block_header.c
test_index_SOURCES = test_index.c
test_info_SOURCES = test_info.c

TESTS = $(check_PROGRAMS)
