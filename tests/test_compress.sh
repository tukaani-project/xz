#!/bin/sh

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# If xz wasn't built, this test is skipped.
if test -x ../src/xz/xz ; then
	:
else
	(exit 77)
	exit 77
fi

# Find out if our shell supports functions.
eval 'unset foo ; foo() { return 42; } ; foo'
if test $? != 42 ; then
	echo "/bin/sh doesn't support functions, skipping this test."
	(exit 77)
	exit 77
fi

test_xz() {
	if $XZ -c "$@" "$FILE" > "$TMP_COMP"; then
		:
	else
		echo "Compressing failed: $* $FILE"
		(exit 1)
		exit 1
	fi

	if $XZ -cd "$TMP_COMP" > "$TMP_UNCOMP" ; then
		:
	else
		echo "Decompressing failed: $* $FILE"
		(exit 1)
		exit 1
	fi

	if cmp "$TMP_UNCOMP" "$FILE" ; then
		:
	else
		echo "Decompressed file does not match" \
				"the original: $* $FILE"
		(exit 1)
		exit 1
	fi

	if test -n "$XZDEC" ; then
		if $XZDEC "$TMP_COMP" > "$TMP_UNCOMP" ; then
			:
		else
			echo "Decompressing failed: $* $FILE"
			(exit 1)
			exit 1
		fi

		if cmp "$TMP_UNCOMP" "$FILE" ; then
			:
		else
			echo "Decompressed file does not match" \
					"the original: $* $FILE"
			(exit 1)
			exit 1
		fi
	fi
}

XZ="../src/xz/xz --memlimit-compress=48MiB --memlimit-decompress=5MiB \
		--no-adjust --threads=1 --check=crc64"
XZDEC="../src/xzdec/xzdec" # No memory usage limiter available
test -x ../src/xzdec/xzdec || XZDEC=

# Create the required input file if needed.
FILE=$1
case $FILE in
	compress_generated_*)
		if ./create_compress_files "${FILE#compress_generated_}" ; then
			:
		else
			rm -f "$FILE"
			echo "Failed to create the file '$FILE'."
			(exit 1)
			exit 1
		fi
		;;
	'')
		echo "No test file was specified."
		(exit 1)
		exit 1
		;;
esac

# Derive temporary filenames for compressed and uncompressed outputs
# from the input filename. This is needed when multiple tests are
# run in parallel.
TMP_COMP="tmp_comp_${FILE##*/}"
TMP_UNCOMP="tmp_uncomp_${FILE##*/}"

# Remove temporary now (in case they are something weird), and on exit.
rm -f "$TMP_COMP" "$TMP_UNCOMP"
trap 'rm -f "$TMP_COMP" "$TMP_UNCOMP"' 0

# Compress and decompress the file with various filter configurations.
#
# Don't test with empty arguments; it breaks some ancient
# proprietary /bin/sh versions due to $@ used in test_xz().
test_xz -1
test_xz -2
test_xz -3
test_xz -4

for ARGS in \
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
done

(exit 0)
exit 0
