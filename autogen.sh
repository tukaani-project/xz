#!/bin/sh

set -e -x

autopoint -f
libtoolize -c -f || glibtoolize -c -f
aclocal -I m4
autoconf
autoheader
automake -acf --foreign
