#!/bin/sh

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# If both xz and xzdec were not build, skip this test.
XZ=../src/xz/xz
XZDEC=../src/xzdec/xzdec
test -x "$XZ" || XZ=
test -x "$XZDEC" || XZDEC=
if test -z "$XZ$XZDEC"; then
	echo "xz and xzdec were not built, skipping this test."
	exit 77
fi

# If decompression support is missing, this test is skipped.
# This isn't perfect as if only some decompressors are disabled
# then some good files might not decompress and the test fails
# for a (kind of) wrong reason.
if grep 'define HAVE_DECODERS' ../config.h > /dev/null ; then
	:
else
	echo "Decompression support is disabled, skipping this test."
	exit 77
fi


#######
# .xz #
#######

for I in "$srcdir"/files/good-*.xz
do
	if test -z "$XZ" || "$XZ" -dc "$I" > /dev/null; then
		:
	else
		echo "Good file failed: $I"
		exit 1
	fi

	if test -z "$XZDEC" || "$XZDEC" "$I" > /dev/null; then
		:
	else
		echo "Good file failed: $I"
		exit 1
	fi
done

for I in "$srcdir"/files/bad-*.xz
do
	if test -n "$XZ" && "$XZ" -dc "$I" > /dev/null 2>&1; then
		echo "Bad file succeeded: $I"
		exit 1
	fi

	if test -n "$XZDEC" && "$XZDEC" "$I" > /dev/null 2>&1; then
		echo "Bad file succeeded: $I"
		exit 1
	fi
done

# Testing for the lzma_index_append() bug in <= 5.2.6 needs "xz -l":
I="$srcdir/files/bad-3-index-uncomp-overflow.xz"
if test -n "$XZ" && "$XZ" -l "$I" > /dev/null 2>&1; then
	echo "Bad file succeeded with xz -l: $I"
	exit 1
fi

for I in "$srcdir"/files/unsupported-*.xz
do
	# Test these only with xz as unsupported-check.xz will exit
	# successfully with xzdec because it doesn't warn about
	# unsupported check type.
	if test -n "$XZ" && "$XZ" -dc "$I" > /dev/null 2>&1; then
		echo "Unsupported file succeeded: $I"
		exit 1
	fi
done

# Test that this passes with --no-warn (-Q).
I="$srcdir/files/unsupported-check.xz"
if test -z "$XZ" || "$XZ" -dcQ "$I" > /dev/null; then
	:
else
	echo "Unsupported file failed with xz -Q: $I"
	exit 1
fi

if test -z "$XZDEC" || "$XZDEC" -Q "$I" > /dev/null; then
	:
else
	echo "Unsupported file failed with xzdec -Q: $I"
	exit 1
fi


#########
# .lzma #
#########

for I in "$srcdir"/files/good-*.lzma
do
	if test -z "$XZ" || "$XZ" -dc "$I" > /dev/null; then
		:
	else
		echo "Good file failed: $I"
		exit 1
	fi
done

for I in "$srcdir"/files/bad-*.lzma
do
	if test -n "$XZ" && "$XZ" -dc "$I" > /dev/null 2>&1; then
		echo "Bad file succeeded: $I"
		exit 1
	fi
done

exit 0
