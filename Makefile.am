##
##  Copyright (C) 2007 Lasse Collin
##
##  This library is free software; you can redistribute it and/or
##  modify it under the terms of the GNU Lesser General Public
##  License as published by the Free Software Foundation; either
##  version 2.1 of the License, or (at your option) any later version.
##
##  This library is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##  Lesser General Public License for more details.
##

SUBDIRS =

if COND_GNULIB
SUBDIRS += lib
endif

SUBDIRS += src po tests


EXTRA_DIST = \
	m4 \
	config.rpath  \
	Doxyfile.in \
	extra \
	doc \
	COPYING.GPLv2 \
	COPYING.GPLv3 \
	COPYING.LGPLv2.1

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign
