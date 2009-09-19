#
# SYNOPSIS
#
#   TUKLIB_CPUCORES
#
# DESCRIPTION
#
#   Check how to find out the number of available CPU cores in the system.
#   This information is used by tuklib_cpucores.c.
#
#   Currently this supports sysctl() (BSDs, OS/2) and sysconf() (GNU/Linux,
#   Solaris, Cygwin).
#
# COPYING
#
#   Author: Lasse Collin
#
#   This file has been put into the public domain.
#   You can do whatever you want with this file.
#

AC_DEFUN_ONCE([TUKLIB_CPUCORES], [
AC_REQUIRE([TUKLIB_COMMON])

# sys/param.h might be needed by sys/sysctl.h.
AC_CHECK_HEADERS([sys/param.h])

AC_MSG_CHECKING([how to detect the number of available CPU cores])

# Look for sysctl() solution first, because on OS/2, both sysconf()
# and sysctl() pass the tests in this file, but only sysctl()
# actually works.
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main(void)
{
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	sysctl(name, 2, &cpus, &cpus_size, NULL, 0);
	return 0;
}
]])], [
	AC_DEFINE([TUKLIB_CPUCORES_SYSCTL], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysctl().])
	AC_MSG_RESULT([sysctl])
], [

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main(void)
{
	long i;
	i = sysconf(_SC_NPROCESSORS_ONLN);
	return 0;
}
]])], [
	AC_DEFINE([TUKLIB_CPUCORES_SYSCONF], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysconf(_SC_NPROCESSORS_ONLN).])
	AC_MSG_RESULT([sysconf])
], [
	AC_MSG_RESULT([unknown])
])])
])dnl
