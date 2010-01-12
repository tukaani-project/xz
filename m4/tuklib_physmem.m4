#
# SYNOPSIS
#
#   TUKLIB_PHYSMEM
#
# DESCRIPTION
#
#   Check how to get the amount of physical memory.
#   This information is used in tuklib_physmem.c.
#
#   Supported methods:
#
#     - Windows (including Cygwin), OS/2, DJGPP (DOS), and OpenVMS have
#       operating-system specific functions.
#
#     - sysconf() works on GNU/Linux and Solaris, and possibly on
#       some BSDs.
#
#     - BSDs use sysctl().
#
#     - IRIX has setinvent_r(), getinvent_r(), and endinvent_r().
#
#     - sysinfo() works on Linux/dietlibc and probably on other Linux
#       systems whose libc may lack sysconf().
#
# COPYING
#
#   Author: Lasse Collin
#
#   This file has been put into the public domain.
#   You can do whatever you want with this file.
#

AC_DEFUN_ONCE([TUKLIB_PHYSMEM], [
AC_REQUIRE([TUKLIB_COMMON])

# sys/param.h might be needed by sys/sysctl.h.
AC_CHECK_HEADERS([sys/param.h])

AC_CACHE_CHECK([how to detect the amount of physical memory],
	[tuklib_cv_physmem_method], [

# Maybe checking $host_os would be enough but this matches what
# tuklib_physmem.c does.
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__OS2__) \
		|| defined(__DJGPP__) || defined(__VMS)
int main(void) { return 0; }
#else
#error
#endif
]])], [tuklib_cv_physmem_method=special], [

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main(void)
{
	long i;
	i = sysconf(_SC_PAGESIZE);
	i = sysconf(_SC_PHYS_PAGES);
	return 0;
}
]])], [tuklib_cv_physmem_method=sysconf], [

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main(void)
{
	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	sysctl(name, 2, &mem, &mem_ptr_size, NULL, 0);
	return 0;
}
]])], [tuklib_cv_physmem_method=sysctl], [

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <invent.h>
int
main(void)
{
	inv_state_t *st = NULL;
	setinvent_r(&st);
	getinvent_r(st);
	endinvent_r(st);
	return 0;
}
]])], [tuklib_cv_physmem_method=getinvent_r], [

# This version of sysinfo() is Linux-specific. Some non-Linux systems have
# different sysinfo() so we must check $host_os.
case $host_os in
	linux*)
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/sysinfo.h>
int
main(void)
{
	struct sysinfo si;
	sysinfo(&si);
	return 0;
}
		]])], [
			tuklib_cv_physmem_method=sysinfo
		], [
			tuklib_cv_physmem_method=unknown
		])
		;;
	*)
		tuklib_cv_physmem_method=unknown
		;;
esac
])])])])])
case $tuklib_cv_physmem_method in
	sysconf)
		AC_DEFINE([TUKLIB_PHYSMEM_SYSCONF], [1],
			[Define to 1 if the amount of physical memory can
			be detected with sysconf(_SC_PAGESIZE) and
			sysconf(_SC_PHYS_PAGES).])
		;;
	sysctl)
		AC_DEFINE([TUKLIB_PHYSMEM_SYSCTL], [1],
			[Define to 1 if the amount of physical memory can
			be detected with sysctl().])
		;;
	getinvent_r)
		AC_DEFINE([TUKLIB_PHYSMEM_GETINVENT_R], [1],
			[Define to 1 if the amount of physical memory
			can be detected with getinvent_r().])
		;;
	sysinfo)
		AC_DEFINE([TUKLIB_PHYSMEM_SYSINFO], [1],
			[Define to 1 if the amount of physical memory
			can be detected with Linux sysinfo().])
		;;
esac
])dnl
