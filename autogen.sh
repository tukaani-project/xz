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

# The following six lines are almost identical to "autoreconf -fi" but faster.
${AUTOPOINT:-autopoint} -f
${LIBTOOLIZE:-libtoolize} -c -f || glibtoolize -c -f
${ACLOCAL:-aclocal} -I m4
${AUTOCONF:-autoconf}
${AUTOHEADER:-autoheader}
${AUTOMAKE:-automake} -acf --foreign

# Generate the translated man pages if the "po4a" tool is available.
# This is *NOT* done by "autoreconf -fi" or when "make" is run.
cd po4a && sh update-po
