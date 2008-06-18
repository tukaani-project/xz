##
##  This code has been put into the public domain.
##
##  This library is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##  Lesser General Public License for more details.
##

EXTRA_DIST = crc32_tablegen.c crc64_tablegen.c

noinst_LTLIBRARIES = libcheck.la
libcheck_la_SOURCES = \
	check.c \
	check.h \
	check_init.c \
	crc_macros.h
libcheck_la_CPPFLAGS = \
	-I@top_srcdir@/src/liblzma/api \
	-I@top_srcdir@/src/liblzma/common

if COND_CHECK_CRC32

if COND_ASM_X86
libcheck_la_SOURCES += crc32_x86.S
else
libcheck_la_SOURCES += crc32.c
endif

if COND_SMALL
libcheck_la_SOURCES += crc32_init.c
else
libcheck_la_SOURCES += crc32_table.c crc32_table_le.h crc32_table_be.h
endif

endif


if COND_CHECK_CRC64

if COND_ASM_X86
libcheck_la_SOURCES += crc64_x86.S
else
libcheck_la_SOURCES += crc64.c
endif

if COND_SMALL
libcheck_la_SOURCES += crc64_init.c
else
libcheck_la_SOURCES += crc64_table.c crc64_table_le.h crc64_table_be.h
endif

endif


if COND_CHECK_SHA256
libcheck_la_SOURCES += sha256.c
# Hide bogus warning to allow usage of -Werror. If more issues like this
# pop up, we'll drop -Werror.
if COND_WNO_UNINITIALIZED
CFLAGS += -Wno-uninitialized
endif
endif
