///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64.c
/// \brief      CRC64 calculation
///
/// There are two methods in this file. crc64_generic uses the
/// the slice-by-four algorithm. This is the same idea that is
/// used in crc32_fast.c, but for CRC64 we use only four tables
/// instead of eight to avoid increasing CPU cache usage.
///
/// crc64_clmul uses 32/64-bit x86 SSSE3, SSE4.1, and CLMUL instructions.
/// It was derived from
/// https://www.researchgate.net/publication/263424619_Fast_CRC_computation
/// and the public domain code from https://github.com/rawrunprotected/crc
/// (URLs were checked on 2023-09-29).
///
/// FIXME: Builds for 32-bit x86 use crc64_x86.S by default instead
/// of this file and thus CLMUL version isn't available on 32-bit x86
/// unless configured with --disable-assembler. Even then the lookup table
/// isn't omitted in crc64_table.c since it doesn't know that assembly
/// code has been disabled.
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"
#include "crc_common.h"

#ifdef CRC_GENERIC

/////////////////////////////////
// Generic slice-by-four CRC64 //
/////////////////////////////////

#ifdef WORDS_BIGENDIAN
#	define A1(x) ((x) >> 56)
#else
#	define A1 A
#endif


// See the comments in crc32_fast.c. They aren't duplicated here.
static uint64_t
crc64_generic(const uint8_t *buf, size_t size, uint64_t crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = bswap64(crc);
#endif

	if (size > 4) {
		while ((uintptr_t)(buf) & 3) {
			crc = lzma_crc64_table[0][*buf++ ^ A1(crc)] ^ S8(crc);
			--size;
		}

		const uint8_t *const limit = buf + (size & ~(size_t)(3));
		size &= (size_t)(3);

		while (buf < limit) {
#ifdef WORDS_BIGENDIAN
			const uint32_t tmp = (uint32_t)(crc >> 32)
					^ aligned_read32ne(buf);
#else
			const uint32_t tmp = (uint32_t)crc
					^ aligned_read32ne(buf);
#endif
			buf += 4;

			crc = lzma_crc64_table[3][A(tmp)]
			    ^ lzma_crc64_table[2][B(tmp)]
			    ^ S32(crc)
			    ^ lzma_crc64_table[1][C(tmp)]
			    ^ lzma_crc64_table[0][D(tmp)];
		}
	}

	while (size-- != 0)
		crc = lzma_crc64_table[0][*buf++ ^ A1(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
	crc = bswap64(crc);
#endif

	return ~crc;
}
#endif

#if defined(CRC_GENERIC) && defined(CRC_CLMUL)
typedef uint64_t (*crc64_func_type)(
		const uint8_t *buf, size_t size, uint64_t crc);

// Clang 16.0.0 and older has a bug where it marks the ifunc resolver
// function as unused since it is static and never used outside of
// __attribute__((__ifunc__())).
#if defined(HAVE_FUNC_ATTRIBUTE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-function"
#endif

static crc64_func_type
crc64_resolve(void)
{
	return lzma_is_clmul_supported() ? &lzma_crc64_clmul : &crc64_generic;
}

#if defined(HAVE_FUNC_ATTRIBUTE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic pop
#endif

#ifndef HAVE_FUNC_ATTRIBUTE_IFUNC

#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
#	define CRC64_SET_FUNC_ATTR __attribute__((__constructor__))
static crc64_func_type crc64_func;
#else
#	define CRC64_SET_FUNC_ATTR
static uint64_t crc64_dispatch(const uint8_t *buf, size_t size, uint64_t crc);
static crc64_func_type crc64_func = &crc64_dispatch;
#endif


CRC64_SET_FUNC_ATTR
static void
crc64_set_func(void)
{
	crc64_func = crc64_resolve();
	return;
}


#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
static uint64_t
crc64_dispatch(const uint8_t *buf, size_t size, uint64_t crc)
{
	// When __attribute__((__ifunc__(...))) and
	// __attribute__((__constructor__)) isn't supported, set the
	// function pointer without any locking. If multiple threads run
	// the detection code in parallel, they will all end up setting
	// the pointer to the same value. This avoids the use of
	// mythread_once() on every call to lzma_crc64() but this likely
	// isn't strictly standards compliant. Let's change it if it breaks.
	crc64_set_func();
	return crc64_func(buf, size, crc);
}
#endif
#endif
#endif


#ifdef CRC_USE_IFUNC
extern LZMA_API(uint64_t)
lzma_crc64(const uint8_t *buf, size_t size, uint64_t crc)
		__attribute__((__ifunc__("crc64_resolve")));
#else
extern LZMA_API(uint64_t)
lzma_crc64(const uint8_t *buf, size_t size, uint64_t crc)
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
	// it still makes lzma_crc64(buf, 1, crc) 50-100 % slower. When
	// size reaches 12-16 bytes the overhead becomes negligible.
	//
	// So using the generic version for size <= 16 may give better
	// performance with tiny inputs but if such inputs happen rarely
	// it's not so obvious because then the lookup table of the
	// generic version may not be in the processor cache.
#ifdef CRC_USE_GENERIC_FOR_SMALL_INPUTS
	if (size <= 16)
		return crc64_generic(buf, size, crc);
#endif

/*
#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
	// See crc64_dispatch(). This would be the alternative which uses
	// locking and doesn't use crc64_dispatch(). Note that on Windows
	// this method needs Vista threads.
	mythread_once(crc64_set_func);
#endif
*/

	return crc64_func(buf, size, crc);

#elif defined(CRC_CLMUL)
	// If CLMUL is used unconditionally without runtime CPU detection
	// then omitting the generic version and its 8 KiB lookup table
	// makes the library smaller.
	//
	// FIXME: Lookup table isn't currently omitted on 32-bit x86,
	// see crc64_table.c.
	return lzma_crc64_clmul(buf, size, crc);

#else
	return crc64_generic(buf, size, crc);
#endif
}
#endif
