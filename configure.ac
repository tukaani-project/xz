#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

###############################################################################
#
#   Copyright (C) 2007 Lasse Collin
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
###############################################################################

# NOTE: Don't add useless checks. autoscan detects this and that, but don't
# let it confuse you. For example, we don't care about checking for behavior
# of malloc(), stat(), or lstat(), since we don't use those functions in
# a way that would cause the problems the autoconf macros check.

AC_PREREQ(2.61)

# [LZMA] instead of [LZMA utils] since I prefer to have lzma-version.tar.gz
# instead of lzma-utils-version.tar.gz.
AC_INIT([LZMA], [4.42.3alpha], [lasse.collin@tukaani.org])

AC_CONFIG_SRCDIR([src/liblzma/common/common.h])
AC_CONFIG_HEADER([config.h])

echo
echo "LZMA Utils $PACKAGE_VERSION"

echo
echo "System type:"
# This is needed to know if assembler optimizations can be used.
AC_CANONICAL_HOST

echo
echo "Configure options:"

# Enable/disable debugging code:
AC_MSG_CHECKING([if debugging code should be compiled])
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable debugging code.]),
	[], enable_debug=no)
if test "x$enable_debug" = xyes; then
	AC_MSG_RESULT([yes])
else
	AC_DEFINE(NDEBUG, 1, [Define to disable debugging code.])
	AC_MSG_RESULT([no])
fi

# Enable/disable the encoder components:
AC_MSG_CHECKING([if encoder components should be built])
AC_ARG_ENABLE(encoder, AC_HELP_STRING([--disable-encoder],
		[Do not build the encoder components.]),
	[], enable_encoder=yes)
if test "x$enable_encoder" = xyes; then
	AC_DEFINE([HAVE_ENCODER], 1,
		[Define to 1 if encoder components are enabled.])
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(COND_MAIN_ENCODER, test "x$enable_encoder" = xyes)

# Enable/disable the decoder components:
AC_MSG_CHECKING([if decoder components should be built])
AC_ARG_ENABLE(decoder, AC_HELP_STRING([--disable-decoder],
		[Do not build the decoder components.]),
	[], enable_decoder=yes)
if test "x$enable_decoder" = xyes; then
	AC_DEFINE([HAVE_DECODER], 1,
		[Define to 1 if decoder components are enabled.])
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
	if test "x$enable_encoder" = xno; then
		AC_MSG_ERROR([Do not disable both encoder and decoder.])
	fi
fi
AM_CONDITIONAL(COND_MAIN_DECODER, test "x$enable_decoder" = xyes)

# Filters
AC_MSG_CHECKING([which filters to build])
AC_ARG_ENABLE(filters, AC_HELP_STRING([--enable-filters=],
		[Comma-separated list of filters to build. Default=all.
		Filters used in encoding are needed also in decoding.
		Available filters: copy subblock x86 powerpc ia64
		arm armthumb sparc delta lzma]),
		[], [enable_filters=copy,subblock,x86,powerpc,ia64,arm,armthumb,sparc,delta,lzma])
enable_filters=`echo "$enable_filters" | sed 's/,/ /g'`
enable_filters_copy=no
enable_filters_subblock=no
enable_filters_x86=no
enable_filters_powerpc=no
enable_filters_ia64=no
enable_filters_arm=no
enable_filters_armthumb=no
enable_filters_sparc=no
enable_filters_delta=no
enable_filters_lzma=no
enable_simple_filters=no
if test "x$enable_filters" = xno || test "x$enable_filters" = x; then
	AC_MSG_RESULT([])
	AC_MSG_ERROR([Please enable at least one filter.])
else
	for arg in $enable_filters
	do
		case $arg in
			copy)
				enable_filters_copy=yes
				AC_DEFINE([HAVE_FILTER_COPY], 1,
					[Define to 1 if support for the
					Copy filter is enabled.])
				;;
			subblock)
				enable_filters_subblock=yes
				AC_DEFINE([HAVE_FILTER_SUBBLOCK], 1,
					[Define to 1 if support for the
					Subblock filter is enabled.])
				;;
			x86)
				enable_filters_x86=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_X86], 1,
					[Define to 1 if support for the
					x86 (BCJ) filter is enabled.])
				;;
			powerpc)
				enable_filters_powerpc=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_POWERPC], 1,
					[Define to 1 if support for the
					PowerPC filter is enabled.])
				;;
			ia64)
				enable_filters_ia64=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_IA64], 1,
					[Define to 1 if support for the
					IA64 filter is enabled.])
				;;
			arm)
				enable_filters_arm=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_ARM], 1,
					[Define to 1 if support for the
					ARM filter is enabled.])
				;;
			armthumb)
				enable_filters_armthumb=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_ARMTHUMB], 1,
					[Define to 1 if support for the
					ARMThumb filter is enabled.])
				;;
			sparc)
				enable_filters_sparc=yes
				enable_simple_filters=yes
				AC_DEFINE([HAVE_FILTER_SPARC], 1,
					[Define to 1 if support for the
					SPARC filter is enabled.])
				;;
			delta)
				enable_filters_delta=yes
				AC_DEFINE([HAVE_FILTER_DELTA], 1,
					[Define to 1 if support for the
					Delta filter is enabled.])
				;;
			lzma)
				enable_filters_lzma=yes
				AC_DEFINE([HAVE_FILTER_LZMA], 1,
					[Define to 1 if support for the
					LZMA filter is enabled.])
				;;
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown filter: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_filters])
fi
if test "x$enable_simple_filters" = xyes ; then
	AC_DEFINE([HAVE_FILTER_SIMPLE], 1, [Define to 1 if support for any
		of the so called simple filters is enabled.])
fi
AM_CONDITIONAL(COND_FILTER_COPY, test "x$enable_filters_copy" = xyes)
AM_CONDITIONAL(COND_FILTER_SUBBLOCK, test "x$enable_filters_subblock" = xyes)
AM_CONDITIONAL(COND_FILTER_X86, test "x$enable_filters_x86" = xyes)
AM_CONDITIONAL(COND_FILTER_POWERPC, test "x$enable_filters_powerpc" = xyes)
AM_CONDITIONAL(COND_FILTER_IA64, test "x$enable_filters_ia64" = xyes)
AM_CONDITIONAL(COND_FILTER_ARM, test "x$enable_filters_arm" = xyes)
AM_CONDITIONAL(COND_FILTER_ARMTHUMB, test "x$enable_filters_armthumb" = xyes)
AM_CONDITIONAL(COND_FILTER_SPARC, test "x$enable_filters_sparc" = xyes)
AM_CONDITIONAL(COND_FILTER_DELTA, test "x$enable_filters_delta" = xyes)
AM_CONDITIONAL(COND_FILTER_LZMA, test "x$enable_filters_lzma" = xyes)
AM_CONDITIONAL(COND_MAIN_SIMPLE, test "x$enable_simple_filters" = xyes)

# Which match finders should be enabled:
AC_MSG_CHECKING([which match finders to build])
AC_ARG_ENABLE(match-finders, AC_HELP_STRING([--enable-match-finders=],
		[Comma-separated list of match finders to build. Default=all.
		At least one match finder is required for encoding with
		the LZMA filter.
		Available match finders: hc3 hc4 bt2 bt3 bt4]), [],
	[enable_match_finders=hc3,hc4,bt2,bt3,bt4])
enable_match_finders=`echo "$enable_match_finders" | sed 's/,/ /g'`
enable_match_finders_hc3=no
enable_match_finders_hc4=no
enable_match_finders_bt2=no
enable_match_finders_bt3=no
enable_match_finders_bt4=no
if test "x$enable_encoder" = xyes && test "x$enable_filters_lzma" = xyes ; then
	for arg in $enable_match_finders
		do
		case $arg in
			hc3) enable_match_finders_hc3=yes ;;
			hc4) enable_match_finders_hc4=yes ;;
			bt2) enable_match_finders_bt2=yes ;;
			bt3) enable_match_finders_bt3=yes ;;
			bt4) enable_match_finders_bt4=yes ;;
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown match finder: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_match_finders])
else
	AC_MSG_RESULT([(none because not building the LZMA encoder)])
fi
AM_CONDITIONAL(COND_MF_HC3, test "x$enable_match_finders_hc3" = xyes)
AM_CONDITIONAL(COND_MF_HC4, test "x$enable_match_finders_hc4" = xyes)
AM_CONDITIONAL(COND_MF_BT2, test "x$enable_match_finders_bt2" = xyes)
AM_CONDITIONAL(COND_MF_BT3, test "x$enable_match_finders_bt3" = xyes)
AM_CONDITIONAL(COND_MF_BT4, test "x$enable_match_finders_bt4" = xyes)

# Which integrity checks to build
AC_MSG_CHECKING([which integrity checks to build])
AC_ARG_ENABLE(checks, AC_HELP_STRING([--enable-checks=],
		[Comma-separated list of integrity checks to build.
		Default=all. Available integrity checks: crc32 crc64 sha256]),
		[], [enable_checks=crc32,crc64,sha256])
enable_checks=`echo "$enable_checks" | sed 's/,/ /g'`
enable_checks_crc32=no
enable_checks_crc64=no
enable_checks_sha256=no
if test "x$enable_checks" = xno || test "x$enable_checks" = x; then
	AC_MSG_RESULT([(none)])
else
	for arg in $enable_checks
	do
		case $arg in
			crc32)
				enable_checks_crc32=yes
				AC_DEFINE([HAVE_CHECK_CRC32], 1,
					[Define to 1 if CRC32 support
					is enabled.])
				;;
			crc64)
				enable_checks_crc64=yes
				AC_DEFINE([HAVE_CHECK_CRC64], 1,
					[Define to 1 if CRC64 support
					is enabled.])
				;;
			sha256)
				enable_checks_sha256=yes
				AC_DEFINE([HAVE_CHECK_SHA256], 1,
					[Define to 1 if SHA256 support
					is enabled.])
				;;
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown integrity check: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_checks])
fi
if test "x$enable_checks_crc32" = xno ; then
	AC_MSG_ERROR([For now, the CRC32 check must always be enabled.])
fi
AM_CONDITIONAL(COND_CHECK_CRC32, test "x$enable_checks_crc32" = xyes)
AM_CONDITIONAL(COND_CHECK_CRC64, test "x$enable_checks_crc64" = xyes)
AM_CONDITIONAL(COND_CHECK_SHA256, test "x$enable_checks_sha256" = xyes)

# Assembler optimizations
AC_MSG_CHECKING([if assembler optimizations should be used])
AC_ARG_ENABLE(assembler, AC_HELP_STRING([--disable-assembler],
		[Do not use assembler optimizations even if such exist
		for the architecture.]),
		[], [enable_assembler=yes])
if test "x$enable_assembler" = xyes; then
	case $host_cpu in
		i?86)   enable_assembler=x86 ;;
		x86_64) enable_assembler=x86_64 ;;
		*)      enable_assembler=no ;;
	esac
	# Darwin has different ABI than GNU+Linux and Solaris,
	# and the assembler code doesn't assemble.
	case $host_os in
		darwin*) enable_assembler=no ;;
		*)       ;;
	esac
fi
case $enable_assembler in
	x86)
		AC_DEFINE([HAVE_ASM_X86], 1,
			[Define to 1 if using x86 assembler optimizations.])
		;;
	x86_64)
		AC_DEFINE([HAVE_ASM_X86_64], 1,
			[Define to 1 if using x86_64 assembler optimizations.])
		;;
	no)
		;;
	*)
		AC_MSG_RESULT([])
		AC_MSG_ERROR([--enable-assembler accepts only \`yes', \`no', or \`x86'.])
		;;
esac
AC_MSG_RESULT([$enable_assembler])
AM_CONDITIONAL(COND_ASM_X86, test "x$enable_assembler" = xx86)

# Size optimization
AC_MSG_CHECKING([if small size is preferred over speed])
AC_ARG_ENABLE(small, AC_HELP_STRING([--enable-small],
		[Omit precomputed tables to make liblzma a few kilobytes
		smaller. This will increase startup time of applications
		slightly, because the tables need to be computed first.]),
		[], [enable_small=no])
if test "x$enable_small" = xyes; then
	AC_DEFINE([HAVE_SMALL], 1, [Define to 1 if optimizing for size.])
elif test "x$enable_small" != xno; then
	AC_MSG_RESULT([])
	AC_MSG_ERROR([--enable-small accepts only \`yes' or \`no'])
fi
AC_MSG_RESULT([$enable_small])
AM_CONDITIONAL(COND_SMALL, test "x$enable_small" = xyes)

###############################################################################
# Checks for programs.
###############################################################################

echo
echo "Initializing Automake:"

# There's no C++ or Fortran in LZMA Utils:
CXX=no
F77=no

AM_INIT_AUTOMAKE
AC_PROG_LN_S
AM_PROG_CC_C_O
AM_PROG_AS
AC_USE_SYSTEM_EXTENSIONS

echo
echo "Threading support:"
ACX_PTHREAD
CC="$PTHREAD_CC"

echo
echo "Initializing Libtool:"
AC_PROG_LIBTOOL


###############################################################################
# Checks for libraries.
###############################################################################

echo
echo "Initializing gettext:"
AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])

###############################################################################
# Checks for header files.
###############################################################################

echo
echo "System headers and functions:"

# There is currently no workarounds in this package if some of
# these headers are missing.
AC_CHECK_HEADERS([fcntl.h limits.h sys/time.h],
	[],
	[AC_MSG_ERROR([Required header file(s) are missing.])])

# If any of these headers are missing, things should still work correctly:
AC_CHECK_HEADERS([assert.h errno.h byteswap.h sys/param.h sys/sysctl.h],
	[], [], [
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
])


###############################################################################
# Checks for typedefs, structures, and compiler characteristics.
###############################################################################

AC_C_INLINE
AC_C_RESTRICT

AC_HEADER_STDBOOL

AC_TYPE_UINT8_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINTPTR_T

AC_CHECK_SIZEOF([unsigned long])
AC_CHECK_SIZEOF([size_t])

# The command line tool can copy high resolution timestamps if such
# information is availabe in struct stat. Otherwise one second accuracy
# is used. Most systems seem to have st_xtim but BSDs have st_xtimespec.
AC_CHECK_MEMBERS([struct stat.st_atim.tv_nsec, struct stat.st_mtim.tv_nsec,
	struct stat.st_atimespec.tv_nsec, struct stat.st_mtimespec.tv_nsec])

# It is very unlikely that you want to build liblzma without
# large file support.
AC_SYS_LARGEFILE

# At the moment, the only endian-dependent part should be the integrity checks.
AC_C_BIGENDIAN


###############################################################################
# Checks for library functions.
###############################################################################

# Gnulib replacements as needed
gl_GETOPT

# Functions that are not mandatory i.e. we have alternatives for them
# or we can just drop some functionality:
AC_CHECK_FUNCS([memcpy memmove memset futimes futimesat])

# Check how to find out the amount of physical memory in the system. The
# lzma command line tool uses this to automatically limits its memory usage.
# - sysconf() gives all the needed info on GNU+Linux and Solaris.
# - BSDs use sysctl().
AC_MSG_CHECKING([how to detect the amount of physical memory])
AC_COMPILE_IFELSE([
#include <unistd.h>
int
main()
{
	long i;
	i = sysconf(_SC_PAGESIZE);
	i = sysconf(_SC_PHYS_PAGES);
	return 0;
}
], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCONF], 1,
		[Define to 1 if the amount of physical memory can be detected
		with sysconf(_SC_PAGESIZE) and sysconf(_SC_PHYS_PAGES).])
	AC_MSG_RESULT([sysconf])
], [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main()
{
	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	sysctl(name, 2, &mem, &mem_ptr_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCTL], 1,
		[Define to 1 if the amount of physical memory can be detected
		with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
	AC_MSG_RESULT([unknown])
])])

# Check how to find out the number of available CPU cores in the system.
# sysconf(_SC_NPROCESSORS_ONLN) works on most systems, except that BSDs
# use sysctl().
AC_MSG_CHECKING([how to detect the number of available CPU cores])
AC_COMPILE_IFELSE([
#include <unistd.h>
int
main()
{
	long i;
	i = sysconf(_SC_NPROCESSORS_ONLN);
	return 0;
}
], [
	AC_DEFINE([HAVE_NCPU_SYSCONF], 1,
		[Define to 1 if the number of available CPU cores can be
		detected with sysconf(_SC_NPROCESSORS_ONLN).])
	AC_MSG_RESULT([sysconf])
], [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main()
{
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	sysctl(name, 2, &cpus, &cpus_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_NCPU_SYSCTL], 1,
		[Define to 1 if the number of available CPU cores can be
		detected with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
	AC_MSG_RESULT([unknown])
])])


###############################################################################
# If using GCC, set some additional CFLAGS:
###############################################################################

Wno_uninitialized=no

if test "x$GCC" = xyes ; then
	echo
	echo "GCC extensions:"
	gl_VISIBILITY
	if test -n "$CFLAG_VISIBILITY" ; then
		CFLAGS="$CFLAG_VISIBILITY $CFLAGS"
	fi

	# -Wno-uninitialized is needed with -Werror with SHA256 code
	# to omit a bogus warning.
	AC_MSG_CHECKING([if $CC accepts -Wno-uninitialized])
	OLD_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wno-uninitialized"
	AC_COMPILE_IFELSE([void foo(void) { }], [Wno_uninitialized=yes])
	CFLAGS="$OLD_CFLAGS"
	AC_MSG_RESULT([$Wno_uninitialized])

	# Enable as much warnings as possible. These commented warnings won't
	# work for LZMA Utils though:
	#   * -Wunreachable-code breaks several assert(0) cases, which are
	#     backed up with "return LZMA_PROG_ERROR".
	#   * -Wcast-qual would break various things where we need a non-const
	#     pointer although we don't modify anything through it.
	#   * -Wcast-align breaks optimized CRC32 and CRC64 implementation
	#     on some architectures (not on x86), where this warning is bogus,
	#     because we take care of correct alignment.
	#   * -Winline, -Wdisabled-optimization, -Wunsafe-loop-optimizations
	#     don't seem so useful here; at least the last one gives some
	#     warnings which are not bugs.
	#
	# The flags are in reverse order below so they end up in "beautiful"
	# order on the actual command line.
	for NEW_FLAG in \
			-Wredundant-decls \
			-Wmissing-noreturn \
			-Wmissing-declarations \
			-Wmissing-prototypes \
			-Wold-style-definition \
			-Wstrict-prototypes \
			-Waggregate-return \
			-Wwrite-strings \
			-Wbad-function-cast \
			-Wpointer-arith \
			-Wshadow \
			-Wfloat-equal \
			-Wstrict-aliasing=2 \
			-Winit-self \
			-Wformat=2 \
			-Wextra \
			-Wall \
			-pedantic \
			-std=c99
	do
		AC_MSG_CHECKING([if $CC accepts $NEW_FLAG])
		OLD_CFLAGS="$CFLAGS"
		CFLAGS="$NEW_FLAG $CFLAGS"
		AC_COMPILE_IFELSE([void foo(void) { }], [
			AC_MSG_RESULT([yes])
		], [
			CFLAGS="$OLD_CFLAGS"
			AC_MSG_RESULT([no])
		])
	done

	AC_ARG_ENABLE([werror],
		AC_HELP_STRING([--enable-werror], [Enable -Werror to abort
			compilation on all compiler warnings.]),
		[], [enable_werror=no])
	if test "x$enable_werror" = "xyes"; then
		CFLAGS="-Werror $CFLAGS"
	fi
fi

AM_CONDITIONAL([COND_WNO_UNINITIALIZED], test "x$Wno_uninitialized" = "xyes")


###############################################################################
# Create the makefiles and config.h
###############################################################################

echo

# Don't build the lib directory at all if we don't need any replacement
# functions.
AM_CONDITIONAL([COND_GNULIB], test -n "$LIBOBJS")

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	po/Makefile.in
	lib/Makefile
	src/Makefile
	src/liblzma/lzma.pc
	src/liblzma/Makefile
	src/liblzma/api/Makefile
	src/liblzma/common/Makefile
	src/liblzma/check/Makefile
	src/liblzma/lz/Makefile
	src/liblzma/lzma/Makefile
	src/liblzma/simple/Makefile
	src/liblzma/subblock/Makefile
	src/liblzma/rangecoder/Makefile
	src/lzma/Makefile
	src/lzmadec/Makefile
	src/scripts/Makefile
	tests/Makefile
])

AC_OUTPUT
