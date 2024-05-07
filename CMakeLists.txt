#############################################################################
#
# Very limited CMake support for building some parts of XZ Utils
#
# Building static liblzma with MSVC should work. Building shared liblzma.dll
# with MSVC may or may not work (building liblzma_w32res.rc might be broken).
# Building liblzma on a few other platforms should work too but it
# is somewhat experimental and not as portable as using ./configure.
#
# On some platforms this builds also xz and xzdec, but these are
# highly experimental and meant for testing only:
#   - No replacement getopt_long(), libc must have it
#   - No sandboxing support
#   - No translations
#
# Other missing things:
#   - No xzgrep or other scripts or their symlinks
#   - No tests (no test failures either!)
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

cmake_minimum_required(VERSION 3.13...3.29 FATAL_ERROR)

include(CheckSymbolExists)
include(CheckStructHasMember)
include(cmake/tuklib_large_file_support.cmake)
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
#define LZMA_VERSION_MINOR ([0-9]+)\n\
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
# llvm-windres from Clang/LLVM 16.0.6 and older: The \x20 results
# in "XZx20Utils" in the compiled binary. The option --use-temp-file
# makes no difference.
#
# llvm-windres 17.0.0 and later: It emulates GNU windres more accurately, so
# the workarounds used with GNU windres must be used with llvm-windres too.
#
# CMake 3.27 doesn't have CMAKE_RC_COMPILER_ID so we rely on
# CMAKE_C_COMPILER_ID.
if((MINGW OR CYGWIN OR MSYS) AND (
        NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
        CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "17"))
    # Use workarounds with GNU windres and llvm-windres >= 17.0.0. The \x20
    # in PACKAGE_NAME works with gcc and clang too so we don't need to worry
    # how to pass different flags to windres and the C compiler.
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

    # Features:
    HAVE_CHECK_CRC32
    HAVE_CHECK_CRC64
    HAVE_CHECK_SHA256
    HAVE_DECODERS
    HAVE_DECODER_ARM
    HAVE_DECODER_ARMTHUMB
    HAVE_DECODER_DELTA
    HAVE_DECODER_IA64
    HAVE_DECODER_LZMA1
    HAVE_DECODER_LZMA2
    HAVE_DECODER_POWERPC
    HAVE_DECODER_SPARC
    HAVE_DECODER_X86
    HAVE_ENCODERS
    HAVE_ENCODER_ARM
    HAVE_ENCODER_ARMTHUMB
    HAVE_ENCODER_DELTA
    HAVE_ENCODER_IA64
    HAVE_ENCODER_LZMA1
    HAVE_ENCODER_LZMA2
    HAVE_ENCODER_POWERPC
    HAVE_ENCODER_SPARC
    HAVE_ENCODER_X86
    HAVE_MF_BT2
    HAVE_MF_BT3
    HAVE_MF_BT4
    HAVE_MF_HC3
    HAVE_MF_HC4

    # Standard headers and types are available:
    HAVE_STDBOOL_H
    HAVE__BOOL
    HAVE_STDINT_H
    HAVE_INTTYPES_H

    # Disable assert() checks when no build type has been specified. Non-empty
    # build types like "Release" and "Debug" handle this by default.
    $<$<CONFIG:>:NDEBUG>
)

# _GNU_SOURCE and such definitions. This specific macro is special since
# it also adds the definitions to CMAKE_REQUIRED_DEFINITIONS.
tuklib_use_system_extensions(ALL)

# Check for large file support. It's required on some 32-bit platforms and
# even on 64-bit MinGW-w64 to get 64-bit off_t. This can be forced off on
# the CMake command line if needed: -DLARGE_FILE_SUPPORT=OFF
tuklib_large_file_support(ALL)

# This is needed by liblzma and xz.
tuklib_integer(ALL)

# Check for clock_gettime(). Do this before checking for threading so
# that we know there if CLOCK_MONOTONIC is available.
if(NOT WIN32)
    check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)

    if(NOT HAVE_CLOCK_GETTIME)
        # With glibc <= 2.17 or Solaris 10 this needs librt.
        # Add librt for the next check for HAVE_CLOCK_GETTIME. If it is
        # found after including the library, we know that librt is required.
        list(INSERT CMAKE_REQUIRED_LIBRARIES 0 rt)
        check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME_LIBRT)

        # If it was found now, add librt to all targets and keep it in
        # CMAKE_REQUIRED_LIBRARIES for further tests too.
        if(HAVE_CLOCK_GETTIME_LIBRT)
            link_libraries(rt)
        else()
            list(REMOVE_AT CMAKE_REQUIRED_LIBRARIES 0)
        endif()
    endif()

    if(HAVE_CLOCK_GETTIME OR HAVE_CLOCK_GETTIME_LIBRT)
        # Check if CLOCK_MONOTONIC is available for clock_gettime().
        check_symbol_exists(CLOCK_MONOTONIC time.h HAVE_DECL_CLOCK_MONOTONIC)

        # HAVE_DECL_CLOCK_MONOTONIC should always be defined to 0 or 1
        # when clock_gettime is available.
        add_compile_definitions(
            HAVE_CLOCK_GETTIME
            HAVE_DECL_CLOCK_MONOTONIC=$<BOOL:"${HAVE_DECL_CLOCK_MONOTONIC}">
        )
    endif()
endif()

# Threading support:
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
if(CMAKE_USE_WIN32_THREADS_INIT)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Define to 1 when using Windows 95 (and thus XP) compatible threads. This
        # avoids use of features that were added in Windows Vista.
        # This is used for 32-bit x86 builds for compatibility reasons since it
        # makes no measurable difference in performance compared to Vista threads.
        add_compile_definitions(MYTHREAD_WIN95)
    else()
        # Define to 1 when using Windows Vista compatible threads. This uses features
        # that are not available on Windows XP.
        add_compile_definitions(MYTHREAD_VISTA)
    endif()
else()
    add_compile_definitions(MYTHREAD_POSIX)

    # Check if pthread_condattr_setclock() exists to use CLOCK_MONOTONIC.
    if(HAVE_DECL_CLOCK_MONOTONIC)
        list(INSERT CMAKE_REQUIRED_LIBRARIES 0 "${CMAKE_THREAD_LIBS_INIT}")
        check_symbol_exists(pthread_condattr_setclock pthread.h
                            HAVE_PTHREAD_CONDATTR_SETCLOCK)
        tuklib_add_definition_if(ALL HAVE_PTHREAD_CONDATTR_SETCLOCK)
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

if(NOT WIN32)
    # Symbol versioning only affects ELF shared libraries. The option is
    # ignored for static libraries.
    #
    # Determine the default value so that it's always set with
    # shared libraries in mind which helps if the build dir is reconfigured
    # from static to shared libs without resetting the cache variables.
    set(SYMBOL_VERSIONING_DEFAULT OFF)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
            (CMAKE_SYSTEM_PROCESSOR MATCHES "[Mm]icro[Bb]laze" OR
             CMAKE_C_COMPILER_ID STREQUAL "NVHPC"))
        # As a special case, GNU/Linux on MicroBlaze gets the generic
        # symbol versioning because GCC 12 doesn't support the __symver__
        # attribute on MicroBlaze. On Linux, CMAKE_SYSTEM_PROCESSOR comes
        # from "uname -m" for native builds (should be "microblaze") or from
        # the CMake toolchain file (not perfectly standardized but it very
        # likely has "microblaze" in lower case or mixed case somewhere in
        # the string).
        #
        # NVIDIA HPC Compiler doesn't support symbol versioning but
        # it uses the linked from the system so the linker script
        # can still be used to get the generic symbol versioning.
        set(SYMBOL_VERSIONING_DEFAULT "generic")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # GNU/Linux-specific symbol versioning for shared liblzma.
        # This includes a few extra compatibility symbols for RHEL/CentOS 7
        # which are pointless on non-glibc non-Linux systems.
        #
        # Avoid symvers on Linux with non-glibc like musl and uClibc.
        # In Autoconf it's enough to check that $host_os equals linux-gnu
        # instead of, for example, linux-musl. CMake doesn't provide such
        # a method.
        #
        # This check is here for now since it's not strictly required
        # by anything else.
        check_c_source_compiles(
                "#include <features.h>
                #if defined(__GLIBC__) && !defined(__UCLIBC__)
                int main(void) { return 0; }
                #else
                compile error
                #endif
            "
            IS_LINUX_WITH_GLIBC)

        if(IS_LINUX_WITH_GLIBC)
            set(SYMBOL_VERSIONING_DEFAULT "linux")
        endif()

    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(SYMBOL_VERSIONING_DEFAULT "generic")
    endif()

    set(SYMBOL_VERSIONING "${SYMBOL_VERSIONING_DEFAULT}" CACHE STRING
        "Enable ELF shared library symbol versioning (OFF, generic, linux)")

    # Show a dropdown menu in CMake GUI:
    set_property(CACHE SYMBOL_VERSIONING PROPERTY STRINGS "OFF;generic;linux")
endif()


add_library(liblzma
    src/common/mythread.h
    src/common/sysdefs.h
    src/common/tuklib_common.h
    src/common/tuklib_config.h
    src/common/tuklib_cpucores.c
    src/common/tuklib_cpucores.h
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
    src/liblzma/check/crc32_fast.c
    src/liblzma/check/crc32_table.c
    src/liblzma/check/crc32_table_be.h
    src/liblzma/check/crc32_table_le.h
    src/liblzma/check/crc64_fast.c
    src/liblzma/check/crc64_table.c
    src/liblzma/check/crc64_table_be.h
    src/liblzma/check/crc64_table_le.h
    src/liblzma/check/crc_macros.h
    src/liblzma/check/sha256.c
    src/liblzma/common/alone_decoder.c
    src/liblzma/common/alone_decoder.h
    src/liblzma/common/alone_encoder.c
    src/liblzma/common/auto_decoder.c
    src/liblzma/common/block_buffer_decoder.c
    src/liblzma/common/block_buffer_encoder.c
    src/liblzma/common/block_buffer_encoder.h
    src/liblzma/common/block_decoder.c
    src/liblzma/common/block_decoder.h
    src/liblzma/common/block_encoder.c
    src/liblzma/common/block_encoder.h
    src/liblzma/common/block_header_decoder.c
    src/liblzma/common/block_header_encoder.c
    src/liblzma/common/block_util.c
    src/liblzma/common/common.c
    src/liblzma/common/common.h
    src/liblzma/common/easy_buffer_encoder.c
    src/liblzma/common/easy_decoder_memusage.c
    src/liblzma/common/easy_encoder.c
    src/liblzma/common/easy_encoder_memusage.c
    src/liblzma/common/easy_preset.c
    src/liblzma/common/easy_preset.h
    src/liblzma/common/filter_buffer_decoder.c
    src/liblzma/common/filter_buffer_encoder.c
    src/liblzma/common/filter_common.c
    src/liblzma/common/filter_common.h
    src/liblzma/common/filter_decoder.c
    src/liblzma/common/filter_decoder.h
    src/liblzma/common/filter_encoder.c
    src/liblzma/common/filter_encoder.h
    src/liblzma/common/filter_flags_decoder.c
    src/liblzma/common/filter_flags_encoder.c
    src/liblzma/common/hardware_cputhreads.c
    src/liblzma/common/hardware_physmem.c
    src/liblzma/common/index.c
    src/liblzma/common/index.h
    src/liblzma/common/index_decoder.c
    src/liblzma/common/index_encoder.c
    src/liblzma/common/index_encoder.h
    src/liblzma/common/index_hash.c
    src/liblzma/common/memcmplen.h
    src/liblzma/common/outqueue.c
    src/liblzma/common/outqueue.h
    src/liblzma/common/stream_buffer_decoder.c
    src/liblzma/common/stream_buffer_encoder.c
    src/liblzma/common/stream_decoder.c
    src/liblzma/common/stream_decoder.h
    src/liblzma/common/stream_encoder.c
    src/liblzma/common/stream_encoder_mt.c
    src/liblzma/common/stream_flags_common.c
    src/liblzma/common/stream_flags_common.h
    src/liblzma/common/stream_flags_decoder.c
    src/liblzma/common/stream_flags_encoder.c
    src/liblzma/common/vli_decoder.c
    src/liblzma/common/vli_encoder.c
    src/liblzma/common/vli_size.c
    src/liblzma/delta/delta_common.c
    src/liblzma/delta/delta_common.h
    src/liblzma/delta/delta_decoder.c
    src/liblzma/delta/delta_decoder.h
    src/liblzma/delta/delta_encoder.c
    src/liblzma/delta/delta_encoder.h
    src/liblzma/delta/delta_private.h
    src/liblzma/lz/lz_decoder.c
    src/liblzma/lz/lz_decoder.h
    src/liblzma/lz/lz_encoder.c
    src/liblzma/lz/lz_encoder.h
    src/liblzma/lz/lz_encoder_hash.h
    src/liblzma/lz/lz_encoder_hash_table.h
    src/liblzma/lz/lz_encoder_mf.c
    src/liblzma/lzma/fastpos.h
    src/liblzma/lzma/fastpos_table.c
    src/liblzma/lzma/lzma2_decoder.c
    src/liblzma/lzma/lzma2_decoder.h
    src/liblzma/lzma/lzma2_encoder.c
    src/liblzma/lzma/lzma2_encoder.h
    src/liblzma/lzma/lzma_common.h
    src/liblzma/lzma/lzma_decoder.c
    src/liblzma/lzma/lzma_decoder.h
    src/liblzma/lzma/lzma_encoder.c
    src/liblzma/lzma/lzma_encoder.h
    src/liblzma/lzma/lzma_encoder_optimum_fast.c
    src/liblzma/lzma/lzma_encoder_optimum_normal.c
    src/liblzma/lzma/lzma_encoder_presets.c
    src/liblzma/lzma/lzma_encoder_private.h
    src/liblzma/rangecoder/price.h
    src/liblzma/rangecoder/price_table.c
    src/liblzma/rangecoder/range_common.h
    src/liblzma/rangecoder/range_decoder.h
    src/liblzma/rangecoder/range_encoder.h
    src/liblzma/simple/arm.c
    src/liblzma/simple/armthumb.c
    src/liblzma/simple/ia64.c
    src/liblzma/simple/powerpc.c
    src/liblzma/simple/simple_coder.c
    src/liblzma/simple/simple_coder.h
    src/liblzma/simple/simple_decoder.c
    src/liblzma/simple/simple_decoder.h
    src/liblzma/simple/simple_encoder.c
    src/liblzma/simple/simple_encoder.h
    src/liblzma/simple/simple_private.h
    src/liblzma/simple/sparc.c
    src/liblzma/simple/x86.c
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

target_link_libraries(liblzma Threads::Threads)

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

# immintrin.h:
include(CheckIncludeFile)
check_include_file(immintrin.h HAVE_IMMINTRIN_H)
if(HAVE_IMMINTRIN_H)
    target_compile_definitions(liblzma PRIVATE HAVE_IMMINTRIN_H)

    # SSE2 intrinsics:
    include(CheckCSourceCompiles)
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
elseif(BUILD_SHARED_LIBS AND SYMBOL_VERSIONING STREQUAL "linux")
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
elseif(BUILD_SHARED_LIBS AND SYMBOL_VERSIONING STREQUAL "generic")
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
    IMPORT_PREFIX ""
)

# Create liblzma-config-version.cmake. We use this spelling instead of
# liblzmaConfig.cmake to make find_package work in case insensitive manner
# even with case sensitive file systems. This gives more consistent behavior
# between operating systems.
#
# FIXME: SameMajorVersion is correct for stable releases but it is wrong
# for development releases where each release may have incompatible changes.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config-version.cmake"
    VERSION "${liblzma_VERSION}"
    COMPATIBILITY SameMajorVersion)

# Create liblzma-config.cmake.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/liblzma-config.cmake"
"include(CMakeFindDependencyMacro)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_dependency(Threads)

include(\"\${CMAKE_CURRENT_LIST_DIR}/liblzma-targets.cmake\")

if(NOT TARGET LibLZMA::LibLZMA)
    # Be compatible with the spelling used by the FindLibLZMA module. This
    # doesn't use ALIAS because it would make CMake resolve LibLZMA::LibLZMA
    # to liblzma::liblzma instead of keeping the original spelling. Keeping
    # the original spelling is important for good FindLibLZMA compatibility.
    add_library(LibLZMA::LibLZMA INTERFACE IMPORTED)
    set_target_properties(LibLZMA::LibLZMA PROPERTIES
                          INTERFACE_LINK_LIBRARIES liblzma::liblzma)
endif()
")

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

if(HAVE_GETOPT_LONG)
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
        src/xz/list.c
        src/xz/list.h
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
