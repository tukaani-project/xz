#!/bin/sh

set -e -x

# autooint copies all kinds of crap even though we have told in
# configure.ac that we don't want the intl directory. It is able
# to omit the intl directory but still copies the m4 files needed
# only by the stuff in the non-existing intl directory.
autopoint -f
rm -f \
	codeset.m4 \
	glibc2.m4 \
	glibc21.m4 \
	intdiv0.m4 \
	intl.m4 \
	intldir.m4 \
	intmax.m4 \
	inttypes-pri.m4 \
	inttypes_h.m4 \
	lcmessage.m4 \
	lock.m4 \
	longdouble.m4 \
	longlong.m4 \
	printf-posix.m4 \
	size_max.m4 \
	stdint_h.m4 \
	uintmax_t.m4 \
	ulonglong.m4 \
	visibility.m4 \
	wchar_t.m4 \
	wint_t.m4 \
	xsize.m4

libtoolize -c -f || glibtoolize -c -f
aclocal -I m4
autoconf
autoheader
automake -acf --foreign
