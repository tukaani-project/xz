#!/bin/sh
# SPDX-License-Identifier: 0BSD

###############################################################################
#
# Create or update git_commit_info.h if it is out of date. The file contains
# Git repository commit information to be included in --version messages
# and such places. "make clean" or the equivalent should remove the file.
#
# $1 = path to source tree (default is current directory)
# $2 = path/to/git_commit_info.h to create or update;
#      if not provided, print to standard output
#
###############################################################################
#
# Author: Lasse Collin
#
###############################################################################

set -e

SRCDIR=${1:-.}
FILE=$2

COMMIT=
if test -d "$SRCDIR/.git" && type git > /dev/null 2>&1
then
	# Abbreviated commit ID could look prettier, but web search engines
	# won't find anything with those.
	COMMIT=`git -C "$SRCDIR" log -n1 --pretty='%cs %H'`
	COMMIT=" ($COMMIT)"
fi

NEW="#define GIT_COMMIT_INFO \"$COMMIT\""

# If no target file was provided, print the result to standard output.
if test -z "$FILE"
then
	printf '%s\n' "$NEW"
	exit 0
fi

# Check if the file needs to be created or updated. If the contents haven't
# changed, preserve the file timestamp to avoid useless rebuilds.
OLD=
if test -f "$FILE"
then
	OLD=`cat "$FILE"`
fi

if test "x$NEW" != "x$OLD"
then
	rm -f "$FILE"
	printf '%s\n' "$NEW" > "$FILE"
fi
