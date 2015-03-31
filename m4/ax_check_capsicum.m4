# -*- Autoconf -*-

# SYNOPSIS
#
#   AX_CHECK_CAPSICUM([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   This macro searches for an installed Capsicum library, and if found:
#    - calls one of AC_DEFINE([HAVE_CAPSICUM_SYS_CAPSICUM_H]) or
#      AC_DEFINE([HAVE_CAPSICUM_SYS_CAPABILITY_H])
#    - sets CAPSICUM_LIB to the -l option needed to link Capsicum support.
#
#   If either the header file or the library is not found,
#   shell commands 'action-if-not-found' is run.
#
#   If both header file and library are found, shell commands
#   'action-if-found' is run. If 'action-if-found' is not specified, the
#   default action:
#    - calls AC_DEFINE(HAVE_CAPSICUM)
#    - prepends ${CAPSICUM_LIB} to LIBS.
#
#   You should use autoheader to include a definition for the symbols above
#   in a config.h file.
#
#   Sample usage in a C/C++ source is as follows:
#
#     #ifdef HAVE_CAPSICUM
#     # ifdef HAVE_CAPSICUM_SYS_CAPSICUM_H
#     #  include <sys/capsicum.h>
#     # else
#     #  ifdef HAVE_CAPSICUM_SYS_CAPABILITY_H
#     #   include <sys/capability.h>
#     #  endif
#     # endif
#     #endif /* HAVE_CAPSICUM */
#
# LICENSE
#
#   Copyright (c) 2014 Google Inc.
#
#   Copying and distribution of this file, with or without modification,
#   are permitted in any medium without royalty provided the copyright
#   notice and this notice are preserved.  This file is offered as-is,
#   without any warranty.

AU_ALIAS([CHECK_CAPSICUM], [AX_CHECK_CAPSICUM])
AC_DEFUN([AX_CHECK_CAPSICUM],
[AC_CHECK_HEADERS([sys/capability.h sys/capsicum.h])
capsicum_hdrfound=false
# If <sys/capsicum.h> exists (Linux, FreeBSD>=11.x), assume it is the correct header.
if test "x$ac_cv_header_sys_capsicum_h" = "xyes" ; then
   AC_DEFINE([HAVE_CAPSICUM_SYS_CAPSICUM_H],[],[Capsicum functions declared in <sys/capsicum.h>])
   capsicum_hdrfound=true
elif test "x$ac_cv_header_sys_capability_h" = "xyes" ; then
   # Just <sys/capability.h>; on FreeBSD 10.x this covers Capsicum, but on Linux it
   # describes POSIX.1e capabilities.  So check it declares cap_rights_limit.
   AC_CHECK_DECL([cap_rights_limit],
                  [AC_DEFINE([HAVE_CAPSICUM_SYS_CAPABILITY_H],[],[Capsicum functions declared in <sys/capability.h>])
                   capsicum_hdrfound=true],[],
                 [#include <sys/capability.h>])
fi

AC_LANG_PUSH([C])
# FreeBSD >= 10.x has Capsicum functions in libc
capsicum_libfound=false
AC_LINK_IFELSE([AC_LANG_CALL([], [cap_rights_limit])],
               [capsicum_libfound=true],[])
# Linux has Capsicum functions in libcaprights
AC_CHECK_LIB([caprights],[cap_rights_limit],
             [AC_SUBST([CAPSICUM_LIB],[-lcaprights])
              capsicum_libfound=true],[])
AC_LANG_POP([C])

if test "$capsicum_hdrfound" = "true" && test "$capsicum_libfound" = "true"
then
    # If both library and header were found, action-if-found
    m4_ifblank([$1],[
                LIBS="${CAPSICUM_LIB} $LIBS"
                AC_DEFINE([HAVE_CAPSICUM],[],[Capsicum library available])])
else
    # If either header or library was not found, action-if-not-found
    m4_default([$2],[AC_MSG_WARN([Capsicum support not found])])
fi])


