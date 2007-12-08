///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64.c
/// \brief      CRC64 calculation
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"
#include "crc_macros.h"


#ifdef WORDS_BIGENDIAN
#	define A1(x) ((x) >> 56)
#else
#	define A1 A
#endif


// See comments in crc32.c.
extern uint64_t
lzma_crc64(const uint8_t *buf, size_t size, uint64_t crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = bswap_64(crc);
#endif

	if (size > 4) {
		while ((uintptr_t)(buf) & 3) {
			crc = lzma_crc64_table[0][*buf++ ^ A1(crc)] ^ S8(crc);
			--size;
		}

		const uint8_t *const limit = buf + (size & ~(size_t)(3));
		size &= (size_t)(3);

		// Calculate the CRC64 using the slice-by-four algorithm.
		//
		// In contrast to CRC32 code, this one seems to be fastest
		// with -O3 -fomit-frame-pointer.
		while (buf < limit) {
#ifdef WORDS_BIGENDIAN
			const uint32_t tmp = (crc >> 32) ^ *(uint32_t *)(buf);
#else
			const uint32_t tmp = crc ^ *(uint32_t *)(buf);
#endif
			buf += 4;

			// It is critical for performance, that
			// the crc variable is XORed between the
			// two table-lookup pairs.
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
	crc = bswap_64(crc);
#endif

	return ~crc;
}
