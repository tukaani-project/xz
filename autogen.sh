#!/bin/sh

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

set -e -x

autopoint -f
libtoolize -c -f || glibtoolize -c -f
aclocal -I m4
autoconf
autoheader
automake -acf --foreign
