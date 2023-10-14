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

#ifdef CRC_GENERIC

///////////////////
// Generic CRC32 //
///////////////////

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
	return lzma_is_clmul_supported() ? &lzma_crc32_clmul : &crc32_generic;
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


#ifdef CRC_USE_IFUNC
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
	return lzma_crc32_clmul(buf, size, crc);

#else
	return crc32_generic(buf, size, crc);
#endif
}
#endif
