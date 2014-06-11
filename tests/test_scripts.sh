#!/bin/sh

###############################################################################
#
# Author: Jonathan Nieder
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# If scripts weren't built, this test is skipped.
XZ=../src/xz/xz
XZDIFF=../src/scripts/xzdiff
XZGREP=../src/scripts/xzgrep

for i in XZ XZDIFF XZGREP; do
	eval test -x "\$$i" && continue
	(exit 77)
	exit 77
done

PATH=`pwd`/../src/xz:$PATH
export PATH

test -z "$srcdir" && srcdir=.
preimage=$srcdir/files/good-1-check-crc32.xz
samepostimage=$srcdir/files/good-1-check-crc64.xz
otherpostimage=$srcdir/files/good-1-lzma2-1.xz

"$XZDIFF" "$preimage" "$samepostimage" >/dev/null
status=$?
if test "$status" != 0 ; then
	echo "xzdiff with no changes exited with status $status != 0"
	(exit 1)
	exit 1
fi

"$XZDIFF" "$preimage" "$otherpostimage" >/dev/null
status=$?
if test "$status" != 1 ; then
	echo "xzdiff with changes exited with status $status != 1"
	(exit 1)
	exit 1
fi

"$XZDIFF" "$preimage" "$srcdir/files/missing.xz" >/dev/null 2>&1
status=$?
if test "$status" != 2 ; then
	echo "xzdiff with missing operand exited with status $status != 2"
	(exit 1)
	exit 1
fi

# The exit status must be 0 when a match was found at least from one file,
# and 1 when no match was found in any file.
for pattern in el Hello NOMATCH; do
	for opts in "" "-l" "-h" "-H"; do
		"$XZGREP" $opts $pattern \
			"$srcdir/files/good-1-lzma2-1.xz" \
			"$srcdir/files/good-2-lzma2.xz" > /dev/null 2>&1
		status=$?
		test $status = 0 && test $pattern != NOMATCH && continue
		test $status = 1 && test $pattern = NOMATCH && continue
		echo "wrong exit status from xzgrep"
		(exit 1)
		exit 1
	done
done

(exit 0)
exit 0
