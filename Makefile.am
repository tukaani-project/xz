##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

DIST_SUBDIRS = lib src po tests debug
SUBDIRS =

if COND_GNULIB
SUBDIRS += lib
endif

SUBDIRS += src po tests

EXTRA_DIST = \
	version.sh \
	config.rpath \
	Doxyfile.in \
	extra \
	doc \
	windows \
	COPYING.GPLv2 \
	COPYING.GPLv3 \
	COPYING.LGPLv2.1

ACLOCAL_AMFLAGS = -I m4

# This works with GNU tar and gives cleaner package than normal 'make dist'.
mydist:
	TAR_OPTIONS='--owner=0 --group=0 --numeric-owner --mode=u+rw,go+r-w' \
		$(MAKE) dist-gzip
