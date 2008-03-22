##### http://autoconf-archive.cryp.to/ax_c_arithmetic_rshift.html
#
# SYNOPSIS
#
#   AX_C_ARITHMETIC_RSHIFT
#
# DESCRIPTION
#
#   Checks if the right shift operation is arithmetic.
#
#   This macro uses compile-time detection and so is cross-compile
#   ready.
#
# LAST MODIFICATION
#
#   2006-12-12
#
# COPYLEFT
#
#   Copyright (c) 2006 YAMAMOTO Kengo <yamaken AT bp.iij4u.or.jp>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AX_C_ARITHMETIC_RSHIFT], [
  AC_CACHE_CHECK([whether right shift operation is arithmetic],
                 [ax_cv_c_arithmetic_rshift],
                 [AC_COMPILE_IFELSE([[int dummy[((-1 >> 1) < 0) ? 1 : -1];]],
                                    [ax_cv_c_arithmetic_rshift=yes],
                                    [ax_cv_c_arithmetic_rshift=no])])
  if test "x$ax_cv_c_arithmetic_rshift" = xyes; then
    AC_DEFINE([HAVE_ARITHMETIC_RSHIFT], [1],
              [Define to 1 if the right shift operation is arithmetic.])
  fi
])
