#/bin/sh

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

for I in "$srcdir"/files/good-*.xz
do
	if ../src/xzdec/xzdec "$I" > /dev/null 2> /dev/null ; then
		:
	else
		echo "Good file failed: $I"
		(exit 1)
		exit 1
	fi
done

for I in "$srcdir"/files/bad-*.xz
do
	if ../src/xzdec/xzdec "$I" > /dev/null 2> /dev/null ; then
		echo "Bad file succeeded: $I"
		(exit 1)
		exit 1
	fi
done

(exit 0)
exit 0
