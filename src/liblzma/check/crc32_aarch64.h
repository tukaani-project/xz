///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_aarch64.c
/// \brief      CRC32 calculation with aarch64 optimization
//
//  Authors:    Chenxi Mao
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////
#ifdef LZMA_CRC_CRC32_AARCH64_H
#	error crc_arm64_clmul.h was included twice.
#endif
#define LZMA_CRC_CRC32_AARCH64_H
#include <sys/auxv.h>
// EDG-based compilers (Intel's classic compiler and compiler for E2K) can
// define __GNUC__ but the attribute must not be used with them.
// The new Clang-based ICX needs the attribute.
//
// NOTE: Build systems check for this too, keep them in sync with this.
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__EDG__)
#	define crc_attr_target \
        __attribute__((__target__("+crc")))
#else
#	define crc_attr_target
#endif
#ifdef BUILDING_CRC32_AARCH64
crc_attr_target
crc_attr_no_sanitize_address
static uint32_t
crc32_arch_optimized(const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;
	while ((uintptr_t)(buf) & 7) {
		crc = __builtin_aarch64_crc32b(crc, *buf);
		buf++;
		size--;
	}
	for (;size>=8;size-=8,buf+=8) {
		crc = __builtin_aarch64_crc32x(crc, aligned_read64le(buf));
	}
	for (;size>0;size--,buf++)
		crc = __builtin_aarch64_crc32b(crc, *buf);
	return ~crc;
}
#endif
#ifdef BUILDING_CRC64_AARCH64
//FIXME: there is no crc64_arch_optimized implementation,
// to make compiler happy, add crc64_generic here.
#ifdef WORDS_BIGENDIAN
#	define A1(x) ((x) >> 56)
#else
#	define A1 A
#endif
crc_attr_target
crc_attr_no_sanitize_address
static uint64_t
crc64_arch_optimized(const uint8_t *buf, size_t size, uint64_t crc)
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
static inline bool
is_arch_extension_supported(void)
{
	return (getauxval(AT_HWCAP) & HWCAP_CRC32)!=0;
}

