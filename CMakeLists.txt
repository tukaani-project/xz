#############################################################################
#
# Very limited CMake support for building some parts of XZ Utils
#
# For now, this is intended to be useful to build static or shared liblzma
# on Windows with MSVC (to avoid the need to maintain Visual Studio project
# files). Building liblzma on a few other platforms should work too but it
# is somewhat experimental and not as portable as using ./configure.
#
# On some platforms this builds also xz and xzdec, but these are
# highly experimental and meant for testing only:
#   - No large file support on those 32-bit platforms that need it
#   - No replacement getopt_long(), libc must have it
#   - No sandboxing support
#   - No translations
#
# Other missing things:
#   - No xzgrep or other scripts or their symlinks
#   - No xz tests (liblzma tests only)
#
# NOTE: Even if the code compiles without warnings, the end result may be
# different than via ./configure. Specifically, the list of #defines
# may be different (if so, probably this CMakeLists.txt got them wrong).
#
# This file provides the following installation components (if you only
# need liblzma, install only its components!):
#   - liblzma_Runtime
#   - liblzma_Development
#   - xz (on some platforms only)
#   - xzdec (on some platforms only)
#
# To find the target liblzma::liblzma from other packages, use the CONFIG
# option with find_package() to avoid a conflict with the FindLibLZMA module
# with case-insensitive file systems. For example, to require liblzma 5.2.5
# or a newer compatible version:
#
#     find_package(liblzma 5.2.5 REQUIRED CONFIG)
#     target_link_libraries(my_application liblzma::liblzma)
#
#############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
#############################################################################

cmake_minimum_required(VERSION 3.13...3.26 FATAL_ERROR)

include(CMakePushCheckState)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckCSourceCompiles)
include(cmake/tuklib_integer.cmake)
include(cmake/tuklib_cpucores.cmake)
include(cmake/tuklib_physmem.cmake)
include(cmake/tuklib_progname.cmake)
include(cmake/tuklib_mbstr.cmake)

# Get the package version from version.h into XZ_VERSION variable.
file(READ src/liblzma/api/lzma/version.h XZ_VERSION)
string(REGEX REPLACE
"^.*\n\
#define LZMA_VERSION_MAJOR ([0-9]+)\n\
.*\
#define LZMA_VERSION_MINOR ([0-9]+)\n\
.*\
#define LZMA_VERSION_PATCH ([0-9]+)\n\
.*$"
       "\\1.\\2.\\3" XZ_VERSION "${XZ_VERSION}")

# Among other things, this gives us variables xz_VERSION and xz_VERSION_MAJOR.
project(xz VERSION "${XZ_VERSION}" LANGUAGES C)

# We need a compiler that supports enough C99 or newer (variable-length arrays
# aren't needed, those are optional in C17). Setting CMAKE_C_STANDARD here
# makes it the default for all targets. It doesn't affect the INTERFACE so
# liblzma::liblzma won't end up with INTERFACE_COMPILE_FEATURES "c_std_99"
# (the API headers are C89 and C++ compatible).
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# On Apple OSes, don't build executables as bundles:
set(CMAKE_MACOSX_BUNDLE OFF)

# windres from GNU binutils can be tricky with command line arguments
# that contain spaces or other funny characters. Unfortunately we need
# a space in PACKAGE_NAME. Using \x20 to encode the US-ASCII space seems
# to work in both cmd.exe and /bin/sh.
#
# However, even \x20 isn't enough in all situations, resulting in
# "syntax error" from windres. Using --use-temp-file prevents windres
# from using popen() and this seems to fix the problem.
#
# llvm-windres claims to be compatible with GNU windres but with that
# the \x20 results in "XZx20Utils" in the compiled binary. (At the
# same time it works correctly with clang (the C compiler).) The option
# --use-temp-file makes no difference.
#
# CMake 3.25 doesn't have CMAKE_RC_COMPILER_ID so we rely on
# CMAKE_C_COMPILER_ID. If Clang is used together with GNU windres
# then it will fail, but this way the risk of a bad string in
# the binary should be fairly low.
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Use workarounds with GNU windres. The \x20 in PACKAGE_NAME works
    # with gcc too so we don't need to worry how to pass different flags
    # to windres and gcc.
    string(APPEND CMAKE_RC_FLAGS " --use-temp-file")
    set(PACKAGE_NAME "XZ\\x20Utils")
else()
    # Elsewhere a space is safe. This also keeps things compatible with
    # EBCDIC in case CMake-based build is ever done on such a system.
    set(PACKAGE_NAME "XZ Utils")
endif()

# Definitions common to all targets:
add_compile_definitions(
    # Package info:
    PACKAGE_NAME="${PACKAGE_NAME}"
    PACKAGE_BUGREPORT="xz@tukaani.org"
    PACKAGE_URL="https://tukaani.org/xz/"

    # Standard headers and types are available:
    HAVE_STDBOOL_H
    HAVE__BOOL
    HAVE_STDINT_H
    HAVE_INTTYPES_H

    # Always enable CRC32 since liblzma should never build without it.
    HAVE_CHECK_CRC32

    # Disable assert() checks when no build type has been specified. Non-empty
    # build types like "Release" and "Debug" handle this by default.
    $<$<CONFIG:>:NDEBUG>
)


######################
# System definitions #
######################

# _GNU_SOURCE and such definitions. This specific macro is special since
# it also adds the definitions to CMAKE_REQUIRED_DEFINITIONS.
tuklib_use_system_extensions(ALL)

# This is needed by liblzma and xz.
tuklib_integer(ALL)

# Check for clock_gettime(). Do this before checking for threading so
# that we know there if CLOCK_MONOTONIC is available.
if(NOT WIN32 AND NOT DEFINED HAVE_CLOCK_GETTIME)
    check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
    if(NOT HAVE_CLOCK_GETTIME)
        # With glibc <= 2.17 or Solaris 10 this needs librt.
        unset(HAVE_CLOCK_GETTIME CACHE)

        list(INSERT CMAKE_REQUIRED_LIBRARIES 0 rt)
        check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)

        # If it was found now, add it to all targets and keep it
        # in CMAKE_REQUIRED_LIBRARIES for further tests too.
        if(HAVE_CLOCK_GETTIME)
            link_libraries(rt)
        else()
            list(REMOVE_AT CMAKE_REQUIRED_LIBRARIES 0)
        endif()
    endif()
    if(HAVE_CLOCK_GETTIME)
        # Check if CLOCK_MONOTONIC is available for clock_gettime().
        check_symbol_exists(CLOCK_MONOTONIC time.h HAVE_CLOCK_MONOTONIC)

        add_compile_definitions(
            HAVE_CLOCK_GETTIME
            HAVE_CLOCK_MONOTONIC
        )
    endif()
endif()

# Options for new enough GCC or Clang on any arch or operating system:
if(CMAKE_C_COMPILER_ID MATCHES GNU|Clang)
    # configure.ac has a long list but it won't be copied here:
    add_compile_options(-Wall -Wextra)
endif()


#############################################################################
# liblzma
#############################################################################

option(BUILD_SHARED_LIBS "Build liblzma as a shared library instead of static")

add_library(liblzma
    src/common/mythread.h
    src/common/sysdefs.h
    src/common/tuklib_common.h
    src/common/tuklib_config.h
    src/common/tuklib_integer.h
    src/common/tuklib_physmem.c
    src/common/tuklib_physmem.h
    src/liblzma/api/lzma.h
    src/liblzma/api/lzma/base.h
    src/liblzma/api/lzma/bcj.h
    src/liblzma/api/lzma/block.h
    src/liblzma/api/lzma/check.h
    src/liblzma/api/lzma/container.h
    src/liblzma/api/lzma/delta.h
    src/liblzma/api/lzma/filter.h
    src/liblzma/api/lzma/hardware.h
    src/liblzma/api/lzma/index.h
    src/liblzma/api/lzma/index_hash.h
    src/liblzma/api/lzma/lzma12.h
    src/liblzma/api/lzma/stream_flags.h
    src/liblzma/api/lzma/version.h
    src/liblzma/api/lzma/vli.h
    src/liblzma/check/check.c
    src/liblzma/check/check.h
    src/liblzma/check/crc_macros.h
    src/liblzma/common/block_util.c
    src/liblzma/common/common.c
    src/liblzma/common/common.h
    src/liblzma/common/easy_preset.c
    src/liblzma/common/easy_preset.h
    src/liblzma/common/filter_common.c
    src/liblzma/common/filter_common.h
    src/liblzma/common/hardware_physmem.c
    src/liblzma/common/index.c
    src/liblzma/common/index.h
    src/liblzma/common/memcmplen.h
    src/liblzma/common/stream_flags_common.c
    src/liblzma/common/stream_flags_common.h
    src/liblzma/common/string_conversion.c
    src/liblzma/common/vli_size.c
)

target_include_directories(liblzma PRIVATE
    src/liblzma/api
    src/liblzma/common
    src/liblzma/check
    src/liblzma/lz
    src/liblzma/rangecoder
    src/liblzma/lzma
    src/liblzma/delta
    src/liblzma/simple
    src/common
)


######################
# Size optimizations #
######################

option(ENABLE_SMALL "Reduce code size at expense of speed. \
This may be useful together with CMAKE_BUILD_TYPE=MinSizeRel.")

if(ENABLE_SMALL)
    add_compile_definitions(HAVE_SMALL)
endif()


##########
# Checks #
##########

set(ADDITIONAL_SUPPORTED_CHECKS crc64 sha256)

set(ADDITIONAL_CHECK_TYPES "${ADDITIONAL_SUPPORTED_CHECKS}" CACHE STRING
    "Additional check types to support (crc32 is always built)")

foreach(CHECK IN LISTS ADDITIONAL_CHECK_TYPES)
    if(NOT CHECK IN_LIST ADDITIONAL_SUPPORTED_CHECKS)
        message(SEND_ERROR "'${CHECK}' is not a supported check type")
    endif()
endforeach()

if(ENABLE_SMALL)
    target_sources(liblzma PRIVATE src/liblzma/check/crc32_small.c)
else()
    target_sources(liblzma PRIVATE
        src/liblzma/check/crc32_fast.c
        src/liblzma/check/crc32_table.c
        src/liblzma/check/crc32_table_be.h
        src/liblzma/check/crc32_table_le.h
    )
endif()

if("crc64" IN_LIST ADDITIONAL_CHECK_TYPES)
    add_compile_definitions("HAVE_CHECK_CRC64")

    if(ENABLE_SMALL)
        target_sources(liblzma PRIVATE src/liblzma/check/crc64_small.c)
    else()
        target_sources(liblzma PRIVATE
            src/liblzma/check/crc64_fast.c
            src/liblzma/check/crc64_table.c
            src/liblzma/check/crc64_table_be.h
            src/liblzma/check/crc64_table_le.h
        )
    endif()
endif()

if("sha256" IN_LIST ADDITIONAL_CHECK_TYPES)
    add_compile_definitions("HAVE_CHECK_SHA256")
    target_sources(liblzma PRIVATE src/liblzma/check/sha256.c)
endif()


#################
# Match finders #
#################

set(SUPPORTED_MATCH_FINDERS hc3 hc4 bt2 bt3 bt4)

set(MATCH_FINDERS "${SUPPORTED_MATCH_FINDERS}" CACHE STRING
    "Match finders to support (at least one is required for LZMA1 or LZMA2)")

foreach(MF IN LISTS MATCH_FINDERS)
    if(MF IN_LIST SUPPORTED_MATCH_FINDERS)
        string(TOUPPER "${MF}" MF_UPPER)
        add_compile_definitions("HAVE_MF_${MF_UPPER}")
    else()
        message(SEND_ERROR "'${MF}' is not a supported match finder")
    endif()
endforeach()


#############
# Threading #
#############

# Supported thread methods:
# ON    - autodetect the best threading method. The autodetection will
#         prefer Windows threading (win95 or vista) over posix if both are
#         available. vista threads will be used over win95 unless it is a
#         32-bit build.
# OFF   - Disable threading.
# posix - Use posix threading, or throw an error if not available.
# win95 - Use Windows win95 threading, or throw an error if not available.
# vista - Use Windows vista threading, or throw an error if not available.
set(SUPPORTED_THREAD_METHODS ON OFF posix win95 vista)

set(ENABLE_THREADS ON CACHE STRING
        "Threading method type to support. Set to 'OFF' to disable threading")

# Create dropdown in CMake GUI since only 1 threading method is possible
# to select in a build.
set_property(CACHE ENABLE_THREADS
        PROPERTY STRINGS "${SUPPORTED_THREAD_METHODS}")

if(NOT ENABLE_THREADS IN_LIST SUPPORTED_THREAD_METHODS)
    message(SEND_ERROR "'${ENABLE_THREADS}' is not a supported thread type")
endif()

if(ENABLE_THREADS)
    # Also set THREADS_PREFER_PTHREAD_FLAG since the flag has no effect
    # for Windows threading.
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    # If both Windows and posix threading are available, prefer Windows.
    if(CMAKE_USE_WIN32_THREADS_INIT AND NOT ENABLE_THREADS STREQUAL "posix")
        if(ENABLE_THREADS STREQUAL "win95"
                OR (ENABLE_THREADS STREQUAL "ON"
                AND CMAKE_SIZEOF_VOID_P EQUAL 4))
            # Use Windows 95 (and thus XP) compatible threads.
            # This avoids use of features that were added in
            # Windows Vista. This is used for 32-bit x86 builds for
            # compatibility reasons since it makes no measurable difference
            # in performance compared to Vista threads.
            #
            # The Win95 threading lacks thread-safe one-time initialization
            # function.
            if (ENABLE_SMALL)
                message(SEND_ERROR "Threading method win95 and ENABLE_SMALL "
                                   "cannot be used at the same time")
            endif()

            add_compile_definitions(MYTHREAD_WIN95)
        else()
            add_compile_definitions(MYTHREAD_VISTA)
        endif()
    elseif(CMAKE_USE_PTHREADS_INIT)
        if(ENABLE_THREADS STREQUAL "posix" OR ENABLE_THREADS STREQUAL "ON")
            # Overwrite ENABLE_THREADS in case it was set to "ON".
            # The threading library only needs to be explicitly linked
            # for posix threads, so this is needed for creating
            # liblzma-config.cmake later.
            set(ENABLE_THREADS "posix")

            target_link_libraries(liblzma Threads::Threads)
            add_compile_definitions(MYTHREAD_POSIX)

            # Check if pthread_condattr_setclock() exists to use CLOCK_MONOTONIC.
            if(HAVE_CLOCK_MONOTONIC)
                list(INSERT CMAKE_REQUIRED_LIBRARIES 0 "${CMAKE_THREAD_LIBS_INIT}")
                check_symbol_exists(pthread_condattr_setclock pthread.h
                                    HAVE_PTHREAD_CONDATTR_SETCLOCK)
                tuklib_add_definition_if(ALL HAVE_PTHREAD_CONDATTR_SETCLOCK)
            endif()
        else()
            message(SEND_ERROR
                    "Windows thread method requested, but a compatible "
                    "library could not be found")
        endif()
    else()
        message(SEND_ERROR "No supported threading library found")
    endif()

    target_sources(liblzma PRIVATE
        src/common/tuklib_cpucores.c
        src/common/tuklib_cpucores.h
        src/liblzma/common/hardware_cputhreads.c
        src/liblzma/common/outqueue.c
        src/liblzma/common/outqueue.h
    )
endif()


############
# Encoders #
############

set(SIMPLE_FILTERS
    x86
    arm
    armthumb
    arm64
    powerpc
    ia64
    sparc
)

# The SUPPORTED_FILTERS are shared between Encoders and Decoders
# since only lzip does not appear in both lists. lzip is a special
# case anyway, so it is handled separately in the Decoders section.
set(SUPPORTED_FILTERS
    lzma1
    lzma2
    delta
    "${SIMPLE_FILTERS}"
)

set(ENCODERS "${SUPPORTED_FILTERS}" CACHE STRING "Encoders to support")

# If LZMA2 is enabled, then LZMA1 must also be enabled.
if(NOT "lzma1" IN_LIST ENCODERS AND "lzma2" IN_LIST ENCODERS)
    message(SEND_ERROR "LZMA2 encoder requires that LZMA1 is also enabled")
endif()

# If LZMA1 is enabled, then at least one match finder must be enabled.
if(MATCH_FINDERS STREQUAL "" AND "lzma1" IN_LIST ENCODERS)
    message(SEND_ERROR "At least 1 match finder is required for an "
                       "LZ-based encoder")
endif()

set(HAVE_DELTA_CODER OFF)
set(SIMPLE_ENCODERS OFF)
set(HAVE_ENCODERS OFF)

foreach(ENCODER IN LISTS ENCODERS)
    if(ENCODER IN_LIST SUPPORTED_FILTERS)
        set(HAVE_ENCODERS ON)

        if(NOT SIMPLE_ENCODERS AND ENCODER IN_LIST SIMPLE_FILTERS)
            set(SIMPLE_ENCODERS ON)
        endif()

        string(TOUPPER "${ENCODER}" ENCODER_UPPER)
        add_compile_definitions("HAVE_ENCODER_${ENCODER_UPPER}")
    else()
        message(SEND_ERROR "'${ENCODER}' is not a supported encoder")
    endif()
endforeach()

if(HAVE_ENCODERS)
    add_compile_definitions(HAVE_ENCODERS)

    target_sources(liblzma PRIVATE
        src/liblzma/common/alone_encoder.c
        src/liblzma/common/block_buffer_encoder.c
        src/liblzma/common/block_buffer_encoder.h
        src/liblzma/common/block_encoder.c
        src/liblzma/common/block_encoder.h
        src/liblzma/common/block_header_encoder.c
        src/liblzma/common/easy_buffer_encoder.c
        src/liblzma/common/easy_encoder.c
        src/liblzma/common/easy_encoder_memusage.c
        src/liblzma/common/filter_buffer_encoder.c
        src/liblzma/common/filter_encoder.c
        src/liblzma/common/filter_encoder.h
        src/liblzma/common/filter_flags_encoder.c
        src/liblzma/common/index_encoder.c
        src/liblzma/common/index_encoder.h
        src/liblzma/common/stream_buffer_encoder.c
        src/liblzma/common/stream_encoder.c
        src/liblzma/common/stream_flags_encoder.c
        src/liblzma/common/vli_encoder.c
    )

    if(ENABLE_THREADS)
        target_sources(liblzma PRIVATE
            src/liblzma/common/stream_encoder_mt.c
        )
    endif()

    if(SIMPLE_ENCODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/simple/simple_encoder.c
            src/liblzma/simple/simple_encoder.h
        )
    endif()

    if("lzma1" IN_LIST ENCODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/lzma/lzma_encoder.c
            src/liblzma/lzma/lzma_encoder.h
            src/liblzma/lzma/lzma_encoder_optimum_fast.c
            src/liblzma/lzma/lzma_encoder_optimum_normal.c
            src/liblzma/lzma/lzma_encoder_private.h
            src/liblzma/lzma/fastpos.h
            src/liblzma/lz/lz_encoder.c
            src/liblzma/lz/lz_encoder.h
            src/liblzma/lz/lz_encoder_hash.h
            src/liblzma/lz/lz_encoder_hash_table.h
            src/liblzma/lz/lz_encoder_mf.c
            src/liblzma/rangecoder/price.h
            src/liblzma/rangecoder/price_table.c
            src/liblzma/rangecoder/range_encoder.h
        )

        if(NOT ENABLE_SMALL)
            target_sources(liblzma PRIVATE src/liblzma/lzma/fastpos_table.c)
        endif()
    endif()

    if("lzma2" IN_LIST ENCODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/lzma/lzma2_encoder.c
            src/liblzma/lzma/lzma2_encoder.h
        )
    endif()

    if("delta" IN_LIST ENCODERS)
        set(HAVE_DELTA_CODER ON)
        target_sources(liblzma PRIVATE
            src/liblzma/delta/delta_encoder.c
            src/liblzma/delta/delta_encoder.h
        )
    endif()
endif()


############
# Decoders #
############

set(DECODERS "${SUPPORTED_FILTERS}" CACHE STRING "Decoders to support")

set(SIMPLE_DECODERS OFF)
set(HAVE_DECODERS OFF)

foreach(DECODER IN LISTS DECODERS)
    if(DECODER IN_LIST SUPPORTED_FILTERS)
        set(HAVE_DECODERS ON)

        if(NOT SIMPLE_DECODERS AND DECODER IN_LIST SIMPLE_FILTERS)
            set(SIMPLE_DECODERS ON)
        endif()

        string(TOUPPER "${DECODER}" DECODER_UPPER)
        add_compile_definitions("HAVE_DECODER_${DECODER_UPPER}")
    else()
        message(SEND_ERROR "'${DECODER}' is not a supported decoder")
    endif()
endforeach()

if(HAVE_DECODERS)
    add_compile_definitions(HAVE_DECODERS)

    target_sources(liblzma PRIVATE
        src/liblzma/common/alone_decoder.c
        src/liblzma/common/alone_decoder.h
        src/liblzma/common/auto_decoder.c
        src/liblzma/common/block_buffer_decoder.c
        src/liblzma/common/block_decoder.c
        src/liblzma/common/block_decoder.h
        src/liblzma/common/block_header_decoder.c
        src/liblzma/common/easy_decoder_memusage.c
        src/liblzma/common/file_info.c
        src/liblzma/common/filter_buffer_decoder.c
        src/liblzma/common/filter_decoder.c
        src/liblzma/common/filter_decoder.h
        src/liblzma/common/filter_flags_decoder.c
        src/liblzma/common/index_decoder.c
        src/liblzma/common/index_decoder.h
        src/liblzma/common/index_hash.c
        src/liblzma/common/stream_buffer_decoder.c
        src/liblzma/common/stream_decoder.c
        src/liblzma/common/stream_flags_decoder.c
        src/liblzma/common/stream_decoder.h
        src/liblzma/common/vli_decoder.c
    )

    if(ENABLE_THREADS)
        target_sources(liblzma PRIVATE
            src/liblzma/common/stream_decoder_mt.c
        )
    endif()

    if(SIMPLE_DECODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/simple/simple_decoder.c
            src/liblzma/simple/simple_decoder.h
        )
    endif()

    if("lzma1" IN_LIST DECODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/lzma/lzma_decoder.c
            src/liblzma/lzma/lzma_decoder.h
            src/liblzma/rangecoder/range_decoder.h
            src/liblzma/lz/lz_decoder.c
            src/liblzma/lz/lz_decoder.h
        )
    endif()

    if("lzma2" IN_LIST DECODERS)
        target_sources(liblzma PRIVATE
            src/liblzma/lzma/lzma2_decoder.c
            src/liblzma/lzma/lzma2_decoder.h
        )
    endif()

    if("delta" IN_LIST DECODERS)
        set(HAVE_DELTA_CODER ON)
        target_sources(liblzma PRIVATE
            src/liblzma/delta/delta_decoder.c
            src/liblzma/delta/delta_decoder.h
        )
    endif()
endif()

# Some sources must appear if the filter is configured as either
# an encoder or decoder.
if("lzma1" IN_LIST ENCODERS OR "lzma1" IN_LIST DECODERS)
    target_sources(liblzma PRIVATE
        src/liblzma/rangecoder/range_common.h
        src/liblzma/lzma/lzma_encoder_presets.c
        src/liblzma/lzma/lzma_common.h
    )
endif()

if(HAVE_DELTA_CODER)
    target_sources(liblzma PRIVATE
        src/liblzma/delta/delta_common.c
        src/liblzma/delta/delta_common.h
        src/liblzma/delta/delta_private.h
    )
endif()

if(SIMPLE_ENCODERS OR SIMPLE_DECODERS)
    target_sources(liblzma PRIVATE
        src/liblzma/simple/simple_coder.c
        src/liblzma/simple/simple_coder.h
        src/liblzma/simple/simple_private.h
    )
endif()

foreach(SIMPLE_CODER IN LISTS SIMPLE_FILTERS)
    if(SIMPLE_CODER IN_LIST ENCODERS OR SIMPLE_CODER IN_LIST DECODERS)
        target_sources(liblzma PRIVATE "src/liblzma/simple/${SIMPLE_CODER}.c")
    endif()
endforeach()


#############
# MicroLZMA #
#############

option(MICROLZMA_ENCODER
       "MicroLZMA encoder (needed by specific applications only)" ON)

option(MICROLZMA_DECODER
       "MicroLZMA decoder (needed by specific applications only)" ON)

if(MICROLZMA_ENCODER)
    if(NOT "lzma1" IN_LIST ENCODERS)
        message(SEND_ERROR "The LZMA1 encoder is required to support the "
                           "MicroLZMA encoder")
    endif()

    target_sources(liblzma PRIVATE src/liblzma/common/microlzma_encoder.c)
endif()

if(MICROLZMA_DECODER)
    if(NOT "lzma1" IN_LIST DECODERS)
        message(SEND_ERROR "The LZMA1 decoder is required to support the "
                           "MicroLZMA decoder")
    endif()

    target_sources(liblzma PRIVATE src/liblzma/common/microlzma_decoder.c)
endif()


#############################
# lzip (.lz) format support #
#############################

option(LZIP_DECODER "Support lzip decoder" ON)

if(LZIP_DECODER)
    # If lzip decoder support is requested, make sure LZMA1 decoder is enabled.
    if(NOT "lzma1" IN_LIST DECODERS)
        message(SEND_ERROR "The LZMA1 decoder is required to support the "
                           "lzip decoder")
    endif()

    add_compile_definitions(HAVE_LZIP_DECODER)

    target_sources(liblzma PRIVATE
        src/liblzma/common/lzip_decoder.c
        src/liblzma/common/lzip_decoder.h
    )
endif()

###

# Put the tuklib functions under the lzma_ namespace.
target_compile_definitions(liblzma PRIVATE TUKLIB_SYMBOL_PREFIX=lzma_)
tuklib_cpucores(liblzma)
tuklib_physmem(liblzma)

# While liblzma can be built without tuklib_cpucores or tuklib_physmem
# modules, the liblzma API functions lzma_cputhreads() and lzma_physmem()
# will then be useless (which isn't too bad but still unfortunate). Since
# I expect the CMake-based builds to be only used on systems that are
# supported by these tuklib modules, problems with these tuklib modules
# are considered a hard error for now. This hopefully helps to catch bugs
# in the CMake versions of the tuklib checks.
if(NOT TUKLIB_CPUCORES_FOUND OR NOT TUKLIB_PHYSMEM_FOUND)
    # Use SEND_ERROR instead of FATAL_ERROR. If someone reports a bug,
    # seeing the results of the remaining checks can be useful too.
    message(SEND_ERROR
            "tuklib_cpucores() or tuklib_physmem() failed. "
            "Unless you really are building for a system where these "
            "modules are not supported (unlikely), this is a bug in the "
            "included cmake/tuklib_*.cmake files that should be fixed. "
            "To build anyway, edit this CMakeLists.txt to ignore this error.")
endif()

# Check for __attribute__((__constructor__)) support.
# This needs -Werror because some compilers just warn
# about this being unsupported.
cmake_push_check_state()
set(CMAKE_REQUIRED_FLAGS "-Werror")
check_c_source_compiles("
        __attribute__((__constructor__))
        static void my_constructor_func(void) { return; }
        int main(void) { return 0; }
    "
    HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR)
cmake_pop_check_state()
tuklib_add_definition_if(liblzma HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR)

# cpuid.h
check_include_file(cpuid.h HAVE_CPUID_H)
tuklib_add_definition_if(liblzma HAVE_CPUID_H)

# immintrin.h:
check_include_file(immintrin.h HAVE_IMMINTRIN_H)
if(HAVE_IMMINTRIN_H)
    target_compile_definitions(liblzma PRIVATE HAVE_IMMINTRIN_H)

    # SSE2 intrinsics:
    check_c_source_compiles("
            #include <immintrin.h>
            int main(void)
            {
                __m128i x = { 0 };
                _mm_movemask_epi8(x);
                return 0;
            }
        "
        HAVE__MM_MOVEMASK_EPI8)
    tuklib_add_definition_if(liblzma HAVE__MM_MOVEMASK_EPI8)

    # CLMUL intrinsic:
    check_c_source_compiles("
            #include <immintrin.h>
            #if defined(__e2k__) && __iset__ < 6
            #   error
            #endif
            #if (defined(__GNUC__) || defined(__clang__)) && !defined(__EDG__)
            __attribute__((__target__(\"ssse3,sse4.1,pclmul\")))
            #endif
            __m128i my_clmul(__m128i a)
            {
                const __m128i b = _mm_set_epi64x(1, 2);
                return _mm_clmulepi64_si128(a, b, 0);
            }
            int main(void) { return 0; }
    "
    HAVE_USABLE_CLMUL)
    tuklib_add_definition_if(liblzma HAVE_USABLE_CLMUL)
endif()

# Support -fvisiblity=hidden when building shared liblzma.
# These lines do nothing on Windows (even under Cygwin).
# HAVE_VISIBILITY should always be defined to 0 or 1.
if(BUILD_SHARED_LIBS)
    set_target_properties(liblzma PROPERTIES C_VISIBILITY_PRESET hidden)
    target_compile_definitions(liblzma PRIVATE HAVE_VISIBILITY=1)
else()
    target_compile_definitions(liblzma PRIVATE HAVE_VISIBILITY=0)
endif()

if(WIN32)
    if(BUILD_SHARED_LIBS)
        # Add the Windows resource file for liblzma.dll.
        target_sources(liblzma PRIVATE src/liblzma/liblzma_w32res.rc)

        set_target_properties(liblzma PROPERTIES
            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/common/common_w32res.rc"
        )

        # Export the public API symbols with __declspec(dllexport).
        target_compile_definitions(liblzma PRIVATE DLL_EXPORT)
    else()
        # Disable __declspec(dllimport) when linking against static liblzma.
        target_compile_definitions(liblzma INTERFACE LZMA_API_STATIC)
    endif()
elseif(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # GNU/Linux-specific symbol versioning for shared liblzma.
    # Note that adding link options doesn't affect static builds
    # but HAVE_SYMBOL_VERSIONS_LINUX must not be used with static builds
    # because it would put symbol versions into the static library which
    # can cause problems. It's clearer if all symver related things are
    # omitted when not building a shared library.
    #
    # NOTE: Set it explicitly to 1 to make it clear that versioning is
    # done unconditionally in the C files.
    target_compile_definitions(liblzma PRIVATE HAVE_SYMBOL_VERSIONS_LINUX=1)
    target_link_options(liblzma PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/liblzma_linux.map"
    )
    set_target_properties(liblzma PROPERTIES
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/liblzma_linux.map"
    )
elseif(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    # Symbol versioning for shared liblzma for non-GNU/Linux.
    # FIXME? What about Solaris?
    target_link_options(liblzma PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/liblzma_generic.map"
    )
    set_target_properties(liblzma PROPERTIES
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/liblzma_generic.map"
    )
endif()

set_target_properties(liblzma PROPERTIES
    # At least for now the package versioning matches the rules used for
    # shared library versioning (excluding development releases) so it is
    # fine to use the package version here.
    SOVERSION "${xz_VERSION_MAJOR}"
    VERSION "${xz_VERSION}"

    # It's liblzma.so or liblzma.dll, not libliblzma.so or lzma.dll.
    # Avoid the name lzma.dll because it would conflict with LZMA SDK.
    PREFIX ""
)

# Create liblzma-config-version.cmake.
#
# FIXME: SameMajorVersion is correct for stable releases but it is wrong
# for development releases where each release may have incompatible changes.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config-version.cmake"
    VERSION "${liblzma_VERSION}"
    COMPATIBILITY SameMajorVersion)

# Create liblzma-config.cmake. We use this spelling instead of
# liblzmaConfig.cmake to make find_package work in case insensitive
# manner even with case sensitive file systems. This gives more consistent
# behavior between operating systems. This optionally includes a dependency
# on a threading library, so the contents are created in two separate parts.
# The "second half" is always needed, so create it first.
set(LZMA_CONFIG_CONTENTS
"include(\"\${CMAKE_CURRENT_LIST_DIR}/liblzma-targets.cmake\")

# Be compatible with the spelling used by the FindLibLZMA module. This
# doesn't use ALIAS because it would make CMake resolve LibLZMA::LibLZMA
# to liblzma::liblzma instead of keeping the original spelling. Keeping
# the original spelling is important for good FindLibLZMA compatibility.
add_library(LibLZMA::LibLZMA INTERFACE IMPORTED)
set_target_properties(LibLZMA::LibLZMA PROPERTIES
                      INTERFACE_LINK_LIBRARIES liblzma::liblzma)
")

if(ENABLE_THREADS STREQUAL "posix")
    set(LZMA_CONFIG_CONTENTS
"include(CMakeFindDependencyMacro)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_dependency(Threads)

${LZMA_CONFIG_CONTENTS}
")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config.cmake"
        "${LZMA_CONFIG_CONTENTS}")

# Set CMAKE_INSTALL_LIBDIR and friends.
include(GNUInstallDirs)

# Install the library binary. The INCLUDES specifies the include path that
# is exported for other projects to use but it doesn't install any files.
install(TARGETS liblzma EXPORT liblzmaTargets
        RUNTIME  DESTINATION "${CMAKE_INSTALL_BINDIR}"
                 COMPONENT liblzma_Runtime
        LIBRARY  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                 COMPONENT liblzma_Runtime
                 NAMELINK_COMPONENT liblzma_Development
        ARCHIVE  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                 COMPONENT liblzma_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Install the liblzma API headers. These use a subdirectory so
# this has to be done as a separate step.
install(DIRECTORY src/liblzma/api/
        COMPONENT liblzma_Development
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.h")

# Install the CMake files that other packages can use to find liblzma.
set(liblzma_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/liblzma"
    CACHE STRING "Path to liblzma's .cmake files")

install(EXPORT liblzmaTargets
        NAMESPACE liblzma::
        FILE liblzma-targets.cmake
        DESTINATION "${liblzma_INSTALL_CMAKEDIR}"
        COMPONENT liblzma_Development)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config-version.cmake"
        DESTINATION "${liblzma_INSTALL_CMAKEDIR}"
        COMPONENT liblzma_Development)


#############################################################################
# getopt_long
#############################################################################

# The command line tools needs this.
check_symbol_exists(getopt_long getopt.h HAVE_GETOPT_LONG)


#############################################################################
# xzdec
#############################################################################

if(HAVE_GETOPT_LONG AND HAVE_DECODERS)
    add_executable(xzdec
        src/common/sysdefs.h
        src/common/tuklib_common.h
        src/common/tuklib_config.h
        src/common/tuklib_exit.c
        src/common/tuklib_exit.h
        src/common/tuklib_gettext.h
        src/common/tuklib_progname.c
        src/common/tuklib_progname.h
        src/xzdec/xzdec.c
    )

    target_include_directories(xzdec PRIVATE
        src/common
        src/liblzma/api
    )

    target_link_libraries(xzdec PRIVATE liblzma)

    if(WIN32)
        # Add the Windows resource file for xzdec.exe.
        target_sources(xzdec PRIVATE src/xzdec/xzdec_w32res.rc)
        set_target_properties(xzdec PROPERTIES
            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/common/common_w32res.rc"
        )
    endif()

    tuklib_progname(xzdec)

    install(TARGETS xzdec
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
                    COMPONENT xzdec)

    if(UNIX)
        install(FILES src/xzdec/xzdec.1
                DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
                COMPONENT xzdec)
    endif()
endif()


#############################################################################
# xz
#############################################################################

if(NOT MSVC AND HAVE_GETOPT_LONG)
    add_executable(xz
        src/common/mythread.h
        src/common/sysdefs.h
        src/common/tuklib_common.h
        src/common/tuklib_config.h
        src/common/tuklib_exit.c
        src/common/tuklib_exit.h
        src/common/tuklib_gettext.h
        src/common/tuklib_integer.h
        src/common/tuklib_mbstr.h
        src/common/tuklib_mbstr_fw.c
        src/common/tuklib_mbstr_width.c
        src/common/tuklib_open_stdxxx.c
        src/common/tuklib_open_stdxxx.h
        src/common/tuklib_progname.c
        src/common/tuklib_progname.h
        src/xz/args.c
        src/xz/args.h
        src/xz/coder.c
        src/xz/coder.h
        src/xz/file_io.c
        src/xz/file_io.h
        src/xz/hardware.c
        src/xz/hardware.h
        src/xz/main.c
        src/xz/main.h
        src/xz/message.c
        src/xz/message.h
        src/xz/mytime.c
        src/xz/mytime.h
        src/xz/options.c
        src/xz/options.h
        src/xz/private.h
        src/xz/signals.c
        src/xz/signals.h
        src/xz/suffix.c
        src/xz/suffix.h
        src/xz/util.c
        src/xz/util.h
    )

    target_include_directories(xz PRIVATE
        src/common
        src/liblzma/api
    )

    if(HAVE_DECODERS)
        target_sources(xz PRIVATE
            src/xz/list.c
            src/xz/list.h
        )
    endif()

    target_link_libraries(xz PRIVATE liblzma)

    target_compile_definitions(xz PRIVATE ASSUME_RAM=128)

    if(WIN32)
        # Add the Windows resource file for xz.exe.
        target_sources(xz PRIVATE src/xz/xz_w32res.rc)
        set_target_properties(xz PROPERTIES
            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/common/common_w32res.rc"
        )
    endif()

    tuklib_progname(xz)
    tuklib_mbstr(xz)

    check_symbol_exists(optreset getopt.h HAVE_OPTRESET)
    tuklib_add_definition_if(xz HAVE_OPTRESET)

    check_symbol_exists(posix_fadvise fcntl.h HAVE_POSIX_FADVISE)
    tuklib_add_definition_if(xz HAVE_POSIX_FADVISE)

    # How to get file time:
    check_struct_has_member("struct stat" st_atim.tv_nsec
                            "sys/types.h;sys/stat.h"
                            HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
    if(HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
        tuklib_add_definitions(xz HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
    else()
        check_struct_has_member("struct stat" st_atimespec.tv_nsec
                                "sys/types.h;sys/stat.h"
                                HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC)
        if(HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC)
            tuklib_add_definitions(xz HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC)
        else()
            check_struct_has_member("struct stat" st_atimensec
                                    "sys/types.h;sys/stat.h"
                                    HAVE_STRUCT_STAT_ST_ATIMENSEC)
            tuklib_add_definition_if(xz HAVE_STRUCT_STAT_ST_ATIMENSEC)
        endif()
    endif()

    # How to set file time:
    check_symbol_exists(futimens "sys/types.h;sys/stat.h" HAVE_FUTIMENS)
    if(HAVE_FUTIMENS)
        tuklib_add_definitions(xz HAVE_FUTIMENS)
    else()
        check_symbol_exists(futimes "sys/time.h" HAVE_FUTIMES)
        if(HAVE_FUTIMES)
            tuklib_add_definitions(xz HAVE_FUTIMES)
        else()
            check_symbol_exists(futimesat "sys/time.h" HAVE_FUTIMESAT)
            if(HAVE_FUTIMESAT)
                tuklib_add_definitions(xz HAVE_FUTIMESAT)
            else()
                check_symbol_exists(utimes "sys/time.h" HAVE_UTIMES)
                if(HAVE_UTIMES)
                    tuklib_add_definitions(xz HAVE_UTIMES)
                else()
                    check_symbol_exists(_futime "sys/utime.h" HAVE__FUTIME)
                    if(HAVE__FUTIME)
                        tuklib_add_definitions(xz HAVE__FUTIME)
                    else()
                        check_symbol_exists(utime "utime.h" HAVE_UTIME)
                        tuklib_add_definition_if(xz HAVE_UTIME)
                    endif()
                endif()
            endif()
        endif()
    endif()

    install(TARGETS xz
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
                    COMPONENT xz)

    if(UNIX)
        install(FILES src/xz/xz.1
                DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
                COMPONENT xz)

        option(CREATE_XZ_SYMLINKS "Create unxz and xzcat symlinks" ON)
        option(CREATE_LZMA_SYMLINKS "Create lzma, unlzma, and lzcat symlinks"
               ON)
        set(XZ_LINKS)

        if(CREATE_XZ_SYMLINKS)
            list(APPEND XZ_LINKS "unxz" "xzcat")
        endif()

        if(CREATE_LZMA_SYMLINKS)
            list(APPEND XZ_LINKS "lzma" "unlzma" "lzcat")
        endif()

        # Create symlinks in the build directory and then install them.
        #
        # The symlinks do not likely need any special extension since
        # even on Windows the symlink can still be executed without
        # the .exe extension.
        foreach(LINK IN LISTS XZ_LINKS)
            add_custom_target("${LINK}" ALL
                "${CMAKE_COMMAND}" -E create_symlink
                    "$<TARGET_FILE_NAME:xz>" "${LINK}"
                BYPRODUCTS "${LINK}"
                VERBATIM)
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LINK}"
                    DESTINATION "${CMAKE_INSTALL_BINDIR}"
                    COMPONENT xz)
            add_custom_target("${LINK}.1" ALL
                "${CMAKE_COMMAND}" -E create_symlink "xz.1" "${LINK}.1"
                BYPRODUCTS "${LINK}.1"
                VERBATIM)
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LINK}.1"
                    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
                    COMPONENT xz)
        endforeach()
    endif()
endif()


#############################################################################
# Tests
#############################################################################

include(CTest)

if(BUILD_TESTING)
    set(LIBLZMA_TESTS
        test_bcj_exact_size
        test_block_header
        test_check
        test_filter_flags
        test_filter_str
        test_hardware
        test_index
        test_index_hash
        test_lzip_decoder
        test_memlimit
        test_stream_flags
        test_vli
    )

    foreach(TEST IN LISTS LIBLZMA_TESTS)
        add_executable("${TEST}" "tests/${TEST}.c")

        target_include_directories("${TEST}" PRIVATE
            src/common
            src/liblzma/api
            src/liblzma
            lib
        )

        target_link_libraries("${TEST}" PRIVATE liblzma)

        # Put the test programs into their own subdirectory so they don't
        # pollute the top-level dir which might contain xz and xzdec.
        set_target_properties("${TEST}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests_bin"
        )

        add_test(NAME "${TEST}"
                 COMMAND "${CMAKE_CURRENT_BINARY_DIR}/tests_bin/${TEST}"
        )

        # Set srcdir environment variable so that the tests find their
        # input files from the source tree.
        #
        # Set the return code for skipped tests to match Automake convention.
        set_tests_properties("${TEST}" PROPERTIES
            ENVIRONMENT "srcdir=${CMAKE_CURRENT_LIST_DIR}/tests"
            SKIP_RETURN_CODE 77
        )
    endforeach()
endif()
