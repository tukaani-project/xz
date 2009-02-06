###############################################################################
#
# Makefile to build XZ Utils using MinGW
#
# Make flags to alter compilation:
#
#   DEBUG=1     Enable assertions. Don't use this for production builds!
#               You may also want to set CFLAGS="-g -O0" to disable
#               optimizations.
#
#   W64=1       Build for 64-bit Windows. Make sure that you have 64-bit
#               MinGW in PATH.
#
#   STATIC=1    TODO: Build static library instead of a DLL.
#
#   WINE=1      Shortcut to set CC and STRIP to use Wine to run Windows
#               versions of MinGW binaries.
#
# The usual CPPFLAGS and CFLAGS are supported too.
#
###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

ifdef W64
CC = x86_64-pc-mingw32-gcc
STRIP = x86_64-pc-mingw32-strip
else
CC = mingw32-gcc
STRIP = strip
endif

SED = sed
MKDIR = mkdir
CP = cp
RM = rm -f

CFLAGS = -g -Wall -Wextra -O2
# CFLAGS = -Wall -Wextra -O3 -fomit-frame-pointer -funroll-loops

ALL_CFLAGS = -std=gnu99 -mms-bitfields

ALL_CPPFLAGS = \
	-I. \
	-I../src/common \
	-I../src/liblzma/api \
	-I../src/liblzma/common \
	-I../src/liblzma/check \
	-I../src/liblzma/rangecoder \
	-I../src/liblzma/lz \
	-I../src/liblzma/lzma \
	-I../src/liblzma/delta \
	-I../src/liblzma/simple \
	-I../src/liblzma/subblock

ALL_CPPFLAGS += -DHAVE_CONFIG_H

# This works with Wine too while using native GNU make, sed, and rm.
ifdef WINE
ifdef W64
CC := wine c:/MinGW64/bin/x86_64-pc-mingw32-gcc
STRIP := wine c:/MinGW64/bin/x86_64-pc-mingw32-strip
else
CC := wine c:/MinGW/bin/gcc
STRIP := wine c:/MinGW/bin/strip
endif
endif

ifdef DEBUG
# Use echo since it works for this purporse on both Windows and POSIX.
STRIP := echo Skipping strip
else
ALL_CPPFLAGS += -DNDEBUG
endif

ALL_CPPFLAGS += $(CPPFLAGS)
ALL_CFLAGS += $(CFLAGS)


################
# Common rules #
################

.PHONY: all clean pkg
all: liblzma.dll xzdec.exe lzmadec.exe xz.exe
clean: liblzma-clean xzdec-clean xz-clean

pkg: all
	$(RM) -r pkg
	$(MKDIR) -p pkg/lib pkg/include/lzma
	$(CP) liblzma.dll xz.exe xzdec.exe lzmadec.exe pkg
	$(CP) liblzma.a liblzma.def pkg/lib
	$(CP) ../src/liblzma/api/lzma.h pkg/include
	$(CP) ../src/liblzma/api/lzma/*.h pkg/include/lzma


###############
# liblzma.dll #
###############

LIBLZMA_SRCS_C = \
	../src/liblzma/common/alone_decoder.c \
	../src/liblzma/common/alone_encoder.c \
	../src/liblzma/common/auto_decoder.c \
	../src/liblzma/common/block_buffer_decoder.c \
	../src/liblzma/common/block_buffer_encoder.c \
	../src/liblzma/common/block_decoder.c \
	../src/liblzma/common/block_encoder.c \
	../src/liblzma/common/block_header_decoder.c \
	../src/liblzma/common/block_header_encoder.c \
	../src/liblzma/common/block_util.c \
	../src/liblzma/common/common.c \
	../src/liblzma/common/easy.c \
	../src/liblzma/common/filter_common.c \
	../src/liblzma/common/filter_decoder.c \
	../src/liblzma/common/filter_encoder.c \
	../src/liblzma/common/filter_flags_decoder.c \
	../src/liblzma/common/filter_flags_encoder.c \
	../src/liblzma/common/index.c \
	../src/liblzma/common/index_decoder.c \
	../src/liblzma/common/index_encoder.c \
	../src/liblzma/common/index_hash.c \
	../src/liblzma/common/stream_buffer_decoder.c \
	../src/liblzma/common/stream_buffer_encoder.c \
	../src/liblzma/common/stream_decoder.c \
	../src/liblzma/common/stream_encoder.c \
	../src/liblzma/common/stream_flags_common.c \
	../src/liblzma/common/stream_flags_decoder.c \
	../src/liblzma/common/stream_flags_encoder.c \
	../src/liblzma/common/vli_decoder.c \
	../src/liblzma/common/vli_encoder.c \
	../src/liblzma/common/vli_size.c \
	../src/liblzma/check/check.c \
	../src/liblzma/check/crc32_table.c \
	../src/liblzma/check/crc64_table.c \
	../src/liblzma/check/sha256.c \
	../src/liblzma/rangecoder/price_table.c \
	../src/liblzma/lz/lz_decoder.c \
	../src/liblzma/lz/lz_encoder.c \
	../src/liblzma/lz/lz_encoder_mf.c \
	../src/liblzma/lzma/fastpos_table.c \
	../src/liblzma/lzma/fastpos_tablegen.c \
	../src/liblzma/lzma/lzma2_decoder.c \
	../src/liblzma/lzma/lzma2_encoder.c \
	../src/liblzma/lzma/lzma_decoder.c \
	../src/liblzma/lzma/lzma_encoder.c \
	../src/liblzma/lzma/lzma_encoder_optimum_fast.c \
	../src/liblzma/lzma/lzma_encoder_optimum_normal.c \
	../src/liblzma/lzma/lzma_encoder_presets.c \
	../src/liblzma/delta/delta_common.c \
	../src/liblzma/delta/delta_decoder.c \
	../src/liblzma/delta/delta_encoder.c \
	../src/liblzma/simple/arm.c \
	../src/liblzma/simple/armthumb.c \
	../src/liblzma/simple/ia64.c \
	../src/liblzma/simple/powerpc.c \
	../src/liblzma/simple/simple_coder.c \
	../src/liblzma/simple/simple_decoder.c \
	../src/liblzma/simple/simple_encoder.c \
	../src/liblzma/simple/sparc.c \
	../src/liblzma/simple/x86.c

LIBLZMA_SRCS_ASM =

ifdef W64
LIBLZMA_SRCS_C += \
	../src/liblzma/check/crc32_fast.c \
	../src/liblzma/check/crc64_fast.c
else
LIBLZMA_SRCS_ASM += \
	../src/liblzma/check/crc32_x86.S \
	../src/liblzma/check/crc64_x86.S
endif

LIBLZMA_OBJS_C = $(LIBLZMA_SRCS_C:.c=.o)
LIBLZMA_OBJS_ASM = $(LIBLZMA_SRCS_ASM:.S=.o)
LIBLZMA_OBJS = $(LIBLZMA_OBJS_C) $(LIBLZMA_OBJS_ASM)

# The sed is needed to remove ordinals from the .def file. I'm not going
# to track the ordinal numbers, so people should link against liblzma.dll
# only by using symbol names.
liblzma.dll: $(LIBLZMA_OBJS)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -shared -o liblzma.dll $(LIBLZMA_OBJS) -Wl,--out-implib,liblzma.a,--output-def,liblzma.def.in
	$(SED) 's/ \+@ *[0-9]\+//' liblzma.def.in > liblzma.def
	$(RM) liblzma.def.in
	$(STRIP) --strip-unneeded liblzma.a
	$(STRIP) --strip-all liblzma.dll

$(LIBLZMA_OBJS_C): %.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

$(LIBLZMA_OBJS_ASM): %.o: %.S
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

.PHONY: liblzma-clean
liblzma-clean:
	-$(RM) $(LIBLZMA_OBJS) liblzma.def.in liblzma.def liblzma.a liblzma.dll


###########################
# xzdec.exe & lzmadec.exe #
###########################

XZDEC_SRCS = ../src/xzdec/xzdec.c

xzdec.exe: liblzma.dll $(XZDEC_SRCS)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(XZDEC_SRCS) -o xzdec.exe liblzma.a
	$(STRIP) --strip-all xzdec.exe

lzmadec.exe: liblzma.dll $(XZDEC_SRCS)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -DLZMADEC $(XZDEC_SRCS) -o lzmadec.exe liblzma.a
	$(STRIP) --strip-all lzmadec.exe

.PHONY: xzdec-clean
xzdec-clean:
	-$(RM) xzdec.exe lzmadec.exe


##########
# xz.exe #
##########

XZ_SRCS = \
	../src/xz/args.c \
	../src/xz/hardware.c \
	../src/xz/io.c \
	../src/xz/main.c \
	../src/xz/message.c \
	../src/xz/options.c \
	../src/xz/process.c \
	../src/xz/signals.c \
	../src/xz/suffix.c \
	../src/xz/util.c

XZ_OBJS = $(XZ_SRCS:.c=.o)

# We need to "fix" the source files which use ' as format character
# in printf() to get thousand separators. Windows doesn't support it.
# It's not in C89 or C99, but it is in POSIX.
$(XZ_OBJS): %.o: %.c
	$(SED) "s/%'/%/g" $< > $(<:.c=-fixed.c)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $(<:.c=-fixed.c)

xz.exe: $(XZ_OBJS)
	$(CC) $(ALL_CFLAGS) $(XZ_OBJS) -o xz.exe liblzma.a
	$(STRIP) --strip-all xz.exe

.PHONY: xz-clean
xz-clean:
	-$(RM) $(XZ_OBJS) $(XZ_SRCS:.c=-fixed.c) xz.exe
