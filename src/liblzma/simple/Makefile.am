##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

noinst_LTLIBRARIES = libsimple.la
libsimple_la_CPPFLAGS = \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_srcdir)/src/liblzma/common
libsimple_la_SOURCES = \
	simple_coder.c \
	simple_coder.h \
	simple_private.h

if COND_ENCODER_SIMPLE
libsimple_la_SOURCES += \
	simple_encoder.c \
	simple_encoder.h
endif

if COND_DECODER_SIMPLE
libsimple_la_SOURCES += \
	simple_decoder.c \
	simple_decoder.h
endif

if COND_FILTER_X86
libsimple_la_SOURCES += x86.c
endif

if COND_FILTER_POWERPC
libsimple_la_SOURCES += powerpc.c
endif

if COND_FILTER_IA64
libsimple_la_SOURCES += ia64.c
endif

if COND_FILTER_ARM
libsimple_la_SOURCES += arm.c
endif

if COND_FILTER_ARMTHUMB
libsimple_la_SOURCES += armthumb.c
endif

if COND_FILTER_SPARC
libsimple_la_SOURCES += sparc.c
endif
