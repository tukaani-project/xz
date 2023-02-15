#!/bin/sh

###############################################################################
#
# Author: Jia Tan
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# If xz wasn't built, this test is skipped.
XZ="../src/xz/xz"
if test -x "$XZ" ; then
	:
else
	echo "xz was not built, skipping this test $XZ"
	exit 77
fi

# If decompression support is missing, this test is skipped.
if grep 'define HAVE_DECODERS' ../config.h > /dev/null ; then
	:
else
	echo "Decompression support is disabled, skipping this test."
	exit 77
fi

# Setup nested directory structure, but first
# delete it first if it already exists
rm -rf xz_recursive_level_one

mkdir -p xz_recursive_level_one/level_two/level_three

FILES="file_one "\
"file_two "\
"level_two/file_one "\
"level_two/file_two "\
"level_two/level_three/file_one "\
"level_two/level_three/file_two "

for FILE in $FILES
do
	cp "$srcdir/files/good-0-empty.xz" "xz_recursive_level_one/$FILE.xz"
done

# Decompress with -r, --recursive option
if "$XZ" -dr xz_recursive_level_one; then
	:
else
	echo "Recursive decompression failed: $*"
	exit 1
fi

# Verify the files were all decompressed.
for FILE in $FILES
do
	if test -e "xz_recursive_level_one/$FILE"; then
		:
	else
		echo "File not decompressed: xz_recursive_level_one/$FILE.xz"
		exit 1
	fi

	# Remove decompressed files to prevent warnings in symlink test.
	rm "xz_recursive_level_one/$FILE"
done

# Create a symlink to a directory to create a loop in the file system
# to test that xz will not have infinite recursion.
ln -s ../ xz_recursive_level_one/level_two/loop_link

# The symlink should cause a warning and skip.
"$XZ" -drf xz_recursive_level_one

if [ $? -ne 2 ]; then
	echo "Recursive decompression did not give warning with symlink: $*"
	exit 1
fi

# Clean up nested directory
rm -rf xz_recursive_level_one
