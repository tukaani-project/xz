///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32.c
/// \brief      CRC32 calculation
///
/// There are two methods in this file.
/// crc32_generic uses the slice-by-eight algorithm.
/// It is explained in this document:
/// http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
/// The code in this file is not the same as in Intel's paper, but
/// the basic principle is identical.
///
/// crc32_clmul uses 32/64-bit x86 SSSE3, SSE4.1, and CLMUL instructions.
/// It was derived from
/// https://www.researchgate.net/publication/263424619_Fast_CRC_computation
/// and the public domain code from https://github.com/rawrunprotected/crc
/// (URLs were checked on 2023-09-29).
///
/// FIXME: Builds for 32-bit x86 use crc32_x86.S by default instead
/// of this file and thus CLMUL version isn't available on 32-bit x86
/// unless configured with --disable-assembler. Even then the lookup table
/// isn't omitted in crc32_table.c since it doesn't know that assembly
/// code has been disabled.
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//              Hans Jansen
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"
#include "crc_common.h"

///////////////////
// Generic CRC32 //
///////////////////
#ifdef CRC_GENERIC


static uint32_t
crc32_generic(const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	if (size > 8) {
		// Fix the alignment, if needed. The if statement above
		// ensures that this won't read past the end of buf[].
		while ((uintptr_t)(buf) & 7) {
			crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);
			--size;
		}

		// Calculate the position where to stop.
		const uint8_t *const limit = buf + (size & ~(size_t)(7));

		// Calculate how many bytes must be calculated separately
		// before returning the result.
		size &= (size_t)(7);

		// Calculate the CRC32 using the slice-by-eight algorithm.
		while (buf < limit) {
			crc ^= aligned_read32ne(buf);
			buf += 4;

			crc = lzma_crc32_table[7][A(crc)]
			    ^ lzma_crc32_table[6][B(crc)]
			    ^ lzma_crc32_table[5][C(crc)]
			    ^ lzma_crc32_table[4][D(crc)];

			const uint32_t tmp = aligned_read32ne(buf);
			buf += 4;

			// At least with some compilers, it is critical for
			// performance, that the crc variable is XORed
			// between the two table-lookup pairs.
			crc = lzma_crc32_table[3][A(tmp)]
			    ^ lzma_crc32_table[2][B(tmp)]
			    ^ crc
			    ^ lzma_crc32_table[1][C(tmp)]
			    ^ lzma_crc32_table[0][D(tmp)];
		}
	}

	while (size-- != 0)
		crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	return ~crc;
}
#endif


/////////////////////
// x86 CLMUL CRC32 //
/////////////////////

#ifdef CRC_CLMUL

#include <immintrin.h>


/*
// These functions were used to generate the constants
// at the top of crc32_clmul().
static uint64_t
calc_lo(uint64_t p, uint64_t a, int n)
{
	uint64_t b = 0; int i;
	for (i = 0; i < n; i++) {
		b = b >> 1 | (a & 1) << (n - 1);
		a = (a >> 1) ^ ((0 - (a & 1)) & p);
	}
	return b;
}

// same as ~crc(&a, sizeof(a), ~0)
static uint64_t
calc_hi(uint64_t p, uint64_t a, int n)
{
	int i;
	for (i = 0; i < n; i++)
		a = (a >> 1) ^ ((0 - (a & 1)) & p);
	return a;
}
*/


// MSVC (VS2015 - VS2022) produces bad 32-bit x86 code from the CLMUL CRC
// code when optimizations are enabled (release build). According to the bug
// report, the ebx register is corrupted and the calculated result is wrong.
// Trying to workaround the problem with "__asm mov ebx, ebx" didn't help.
// The following pragma works and performance is still good. x86-64 builds
// aren't affected by this problem.
//
// NOTE: Another pragma after the function restores the optimizations.
// If the #if condition here is updated, the other one must be updated too.
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) && !defined(__clang__) \
		&& defined(_M_IX86)
#	pragma optimize("g", off)
#endif

// EDG-based compilers (Intel's classic compiler and compiler for E2K) can
// define __GNUC__ but the attribute must not be used with them.
// The new Clang-based ICX needs the attribute.
//
// NOTE: Build systems check for this too, keep them in sync with this.
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__EDG__)
__attribute__((__target__("ssse3,sse4.1,pclmul")))
#endif
static uint32_t
crc32_clmul(const uint8_t *buf, size_t size, uint32_t crc)
{
	// The prototypes of the intrinsics use signed types while most of
	// the values are treated as unsigned here. These warnings in this
	// function have been checked and found to be harmless so silence them.
#if TUKLIB_GNUC_REQ(4, 6) || defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wconversion"
#endif

#ifndef CRC_USE_GENERIC_FOR_SMALL_INPUTS
	// The code assumes that there is at least one byte of input.
	if (size == 0)
		return crc;
#endif

	// uint32_t poly = 0xedb88320;
	uint64_t p = 0x1db710640; // p << 1
	uint64_t mu = 0x1f7011641; // calc_lo(p, p, 32) << 1 | 1
	uint64_t k5 = 0x163cd6124; // calc_hi(p, p, 32) << 1
	uint64_t k4 = 0x0ccaa009e; // calc_hi(p, p, 64) << 1
	uint64_t k3 = 0x1751997d0; // calc_hi(p, p, 128) << 1

	__m128i vfold4 = _mm_set_epi64x(mu, p);
	__m128i vfold8 = _mm_set_epi64x(0, k5);
	__m128i vfold16 = _mm_set_epi64x(k4, k3);

	__m128i v0, v1, v2;

	crc_simd_body(buf,  size, &v0, &v1, vfold16, _mm_cvtsi32_si128(~crc));

	v1 = _mm_xor_si128(
			_mm_clmulepi64_si128(v0, vfold16, 0x10), v1); // xxx0
	v2 = _mm_shuffle_epi32(v1, 0xe7); // 0xx0
	v0 = _mm_slli_epi64(v1, 32);  // [0]
	v0 = _mm_clmulepi64_si128(v0, vfold8, 0x00);
	v0 = _mm_xor_si128(v0, v2);   // [1] [2]
	v2 = _mm_clmulepi64_si128(v0, vfold4, 0x10);
	v2 = _mm_clmulepi64_si128(v2, vfold4, 0x00);
	v0 = _mm_xor_si128(v0, v2);   // [2]
	return ~_mm_extract_epi32(v0, 2);

#if TUKLIB_GNUC_REQ(4, 6) || defined(__clang__)
#	pragma GCC diagnostic pop
#endif
}
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) && !defined(__clang__) \
		&& defined(_M_IX86)
#	pragma optimize("", on)
#endif
#endif

#if defined(CRC_GENERIC) && defined(CRC_CLMUL)
typedef uint32_t (*crc32_func_type)(
		const uint8_t *buf, size_t size, uint32_t crc);

// Clang 16.0.0 and older has a bug where it marks the ifunc resolver
// function as unused since it is static and never used outside of
// __attribute__((__ifunc__())).
#if defined(HAVE_FUNC_ATTRIBUTE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-function"
#endif

static crc32_func_type
crc32_resolve(void)
{
	return is_clmul_supported() ? &crc32_clmul : &crc32_generic;
}

#if defined(HAVE_FUNC_ATTRIBUTE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic pop
#endif

#ifndef HAVE_FUNC_ATTRIBUTE_IFUNC

#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
#	define CRC32_SET_FUNC_ATTR __attribute__((__constructor__))
static crc32_func_type crc32_func;
#else
#	define CRC32_SET_FUNC_ATTR
static uint32_t crc32_dispatch(const uint8_t *buf, size_t size, uint32_t crc);
static crc32_func_type crc32_func = &crc32_dispatch;
#endif

CRC32_SET_FUNC_ATTR
static void
crc32_set_func(void)
{
	crc32_func = crc32_resolve();
	return;
}

#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
static uint32_t
crc32_dispatch(const uint8_t *buf, size_t size, uint32_t crc)
{
	// When __attribute__((__ifunc__(...))) and
	// __attribute__((__constructor__)) isn't supported, set the
	// function pointer without any locking. If multiple threads run
	// the detection code in parallel, they will all end up setting
	// the pointer to the same value. This avoids the use of
	// mythread_once() on every call to lzma_crc32() but this likely
	// isn't strictly standards compliant. Let's change it if it breaks.
	crc32_set_func();
	return crc32_func(buf, size, crc);
}

#endif
#endif
#endif


#if defined(CRC_GENERIC) && defined(CRC_CLMUL) \
		&& defined(HAVE_FUNC_ATTRIBUTE_IFUNC)
extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
		__attribute__((__ifunc__("crc32_resolve")));
#else
extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
#if defined(CRC_GENERIC) && defined(CRC_CLMUL)
	// If CLMUL is available, it is the best for non-tiny inputs,
	// being over twice as fast as the generic slice-by-four version.
	// However, for size <= 16 it's different. In the extreme case
	// of size == 1 the generic version can be five times faster.
	// At size >= 8 the CLMUL starts to become reasonable. It
	// varies depending on the alignment of buf too.
	//
	// The above doesn't include the overhead of mythread_once().
	// At least on x86-64 GNU/Linux, pthread_once() is very fast but
	// it still makes lzma_crc32(buf, 1, crc) 50-100 % slower. When
	// size reaches 12-16 bytes the overhead becomes negligible.
	//
	// So using the generic version for size <= 16 may give better
	// performance with tiny inputs but if such inputs happen rarely
	// it's not so obvious because then the lookup table of the
	// generic version may not be in the processor cache.
#ifdef CRC_USE_GENERIC_FOR_SMALL_INPUTS
	if (size <= 16)
		return crc32_generic(buf, size, crc);
#endif

	return crc32_func(buf, size, crc);

#elif defined(CRC_CLMUL)
	return crc32_clmul(buf, size, crc);

#else
	return crc32_generic(buf, size, crc);
#endif
}
#endif
