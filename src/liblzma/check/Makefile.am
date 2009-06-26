##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

EXTRA_DIST = crc32_tablegen.c crc64_tablegen.c

noinst_LTLIBRARIES = libcheck.la
libcheck_la_SOURCES = \
	check.c \
	check.h \
	crc_macros.h
libcheck_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common

if COND_CHECK_CRC32
if COND_SMALL
libcheck_la_SOURCES += crc32_small.c
else
libcheck_la_SOURCES += crc32_table.c crc32_table_le.h crc32_table_be.h
if COND_ASM_X86
libcheck_la_SOURCES += crc32_x86.S
else
libcheck_la_SOURCES += crc32_fast.c
endif
endif
endif

if COND_CHECK_CRC64
if COND_SMALL
libcheck_la_SOURCES += crc64_small.c
else
libcheck_la_SOURCES += crc64_table.c crc64_table_le.h crc64_table_be.h
if COND_ASM_X86
libcheck_la_SOURCES += crc64_x86.S
else
libcheck_la_SOURCES += crc64_fast.c
endif
endif
endif

if COND_CHECK_SHA256
libcheck_la_SOURCES += sha256.c
endif
