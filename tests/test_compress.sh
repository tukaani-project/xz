#!/bin/sh

###############################################################################
#
#   Copyright (C) 2008 Lasse Collin
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
###############################################################################

# Find out if our shell supports functions.
eval 'unset foo ; foo() { return 42; } ; foo'
if test $? != 42 ; then
	echo "/bin/sh doesn't support functions, skipping this test."
	(exit 77)
	exit 77
fi

test_xz() {
	if $XZ -c "$@" "$FILE" > tmp_compressed; then
		:
	else
		echo "Compressing failed: $* $FILE"
		(exit 1)
		exit 1
	fi

	if $XZ -cd tmp_compressed > tmp_uncompressed ; then
		:
	else
		echo "Decoding failed: $* $FILE"
		(exit 1)
		exit 1
	fi

	if cmp tmp_uncompressed "$FILE" ; then
		:
	else
		echo "Decoded file does not match the original: $* $FILE"
		(exit 1)
		exit 1
	fi

	if $XZDEC tmp_compressed > tmp_uncompressed ; then
		:
	else
		echo "Decoding failed: $* $FILE"
		(exit 1)
		exit 1
	fi

	if cmp tmp_uncompressed "$FILE" ; then
		:
	else
		echo "Decoded file does not match the original: $* $FILE"
		(exit 1)
		exit 1
	fi

	# Show progress:
	echo . | tr -d '\n\r'
}

XZ="../src/xz/xz --memory=15MiB --threads=1"
XZDEC="../src/xzdec/xzdec --memory=4MiB"
unset XZ_OPT

# Create the required input files.
if ./create_compress_files ; then
	:
else
	rm -f compress_*
	echo "Failed to create files to test compression."
	(exit 1)
	exit 1
fi

# Remove temporary now (in case they are something weird), and on exit.
rm -f tmp_compressed tmp_uncompressed
trap 'rm -f tmp_compressed tmp_uncompressed' 0

# Encode and decode each file with various filter configurations.
# This takes quite a bit of time.
echo "test_compress.sh:"
for FILE in compress_generated_* "$srcdir"/compress_prepared_*
do
	MSG=`echo "x$FILE" | sed 's,^x,,; s,^.*/,,; s,^compress_,,'`
	echo "  $MSG" | tr -d '\n\r'

	# Don't test with empty arguments; it breaks some ancient
	# proprietary /bin/sh versions due to $@ used in test_xz().
	test_xz -1
	test_xz -2
	test_xz -3
	test_xz -4

	for ARGS in \
		--subblock \
		--subblock=size=1 \
		--subblock=size=1,rle=1 \
		--subblock=size=1,rle=4 \
		--subblock=size=4,rle=4 \
		--subblock=size=8,rle=4 \
		--subblock=size=8,rle=8 \
		--subblock=size=4096,rle=12 \
		--delta=dist=1 \
		--delta=dist=4 \
		--delta=dist=256 \
		--x86 \
		--powerpc \
		--ia64 \
		--arm \
		--armthumb \
		--sparc
	do
		test_xz $ARGS --lzma2=dict=64KiB,nice=32,mode=fast
		test_xz --subblock $ARGS --lzma2=dict=64KiB,nice=32,mode=fast
	done

	echo
done

(exit 0)
exit 0
