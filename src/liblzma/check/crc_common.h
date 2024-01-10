///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc_common.h
/// \brief      Some functions and macros for CRC32 and CRC64
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//              Hans Jansen
//              Jia Tan
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_CRC_COMMON_H
#define LZMA_CRC_COMMON_H

#include "common.h"


#ifdef WORDS_BIGENDIAN
#	define A(x) ((x) >> 24)
#	define B(x) (((x) >> 16) & 0xFF)
#	define C(x) (((x) >> 8) & 0xFF)
#	define D(x) ((x) & 0xFF)

#	define S8(x) ((x) << 8)
#	define S32(x) ((x) << 32)

#else
#	define A(x) ((x) & 0xFF)
#	define B(x) (((x) >> 8) & 0xFF)
#	define C(x) (((x) >> 16) & 0xFF)
#	define D(x) ((x) >> 24)

#	define S8(x) ((x) >> 8)
#	define S32(x) ((x) >> 32)
#endif


// CRC CLMUL code needs this because accessing input buffers that aren't
// aligned to the vector size will inherently trip the address sanitizer.
#if lzma_has_attribute(__no_sanitize_address__)
#	define crc_attr_no_sanitize_address \
			__attribute__((__no_sanitize_address__))
#else
#	define crc_attr_no_sanitize_address
#endif


// The inline keyword is only a suggestion to the compiler to substitute the
// body of the function into the places where it is called. If a function
// is large and called multiple times then compiler may choose to ignore the
// inline suggestion at a sometimes high performance cost.
//
// MSVC's __forceinline is a keyword that should be used in place of inline.
// If both __forceinline and inline are used, MSVC will issue a warning.
// Since MSVC's keyword is a replacement keyword, the lzma_always_inline
// macro must also contain the inline keyword when its not used in MSVC.
//
// NOTE: This doesn't use lzma_always_inline for now as support for it is
// detected using preprocessor macros which might miss a compiler that
// does support it. All compilers that support the CLMUL code support
// the attribute too; if not, we will hopefully get a bug report.
#ifdef _MSC_VER
#	define crc_always_inline __forceinline
#else
#	define crc_always_inline __attribute__((__always_inline__)) inline
#endif

#undef CRC_GENERIC
#undef CRC_CLMUL
#undef CRC_USE_IFUNC
#undef CRC_USE_GENERIC_FOR_SMALL_INPUTS

// If CLMUL cannot be used then only the generic slice-by-eight (CRC32)
// or slice-by-four (CRC64) is built.
#if !defined(HAVE_USABLE_CLMUL)
#	define CRC_GENERIC 1

// If CLMUL is allowed unconditionally in the compiler options then the
// generic version can be omitted. Note that this doesn't work with MSVC
// as I don't know how to detect the features here.
//
// NOTE: Keep this this in sync with crc32_table.c.
#elif (defined(__SSSE3__) && defined(__SSE4_1__) && defined(__PCLMUL__)) \
		|| (defined(__e2k__) && __iset__ >= 6)
#	define CRC_CLMUL 1

// Otherwise build both and detect at runtime which version to use.
#else
#	define CRC_GENERIC 1
#	define CRC_CLMUL 1

#	ifdef HAVE_FUNC_ATTRIBUTE_IFUNC
#		define CRC_USE_IFUNC 1
#	endif

/*
	// The generic code is much faster with 1-8-byte inputs and has
	// similar performance up to 16 bytes  at least in microbenchmarks
	// (it depends on input buffer alignment too). If both versions are
	// built, this #define will use the generic version for inputs up to
	// 16 bytes and CLMUL for bigger inputs. It saves a little in code
	// size since the special cases for 0-16-byte inputs will be omitted
	// from the CLMUL code.
#	ifndef CRC_USE_IFUNC
#		define CRC_USE_GENERIC_FOR_SMALL_INPUTS 1
#	endif
*/
#endif

#endif
