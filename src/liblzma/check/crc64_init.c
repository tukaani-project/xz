///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_init.c
/// \brief      CRC64 table initialization
//
//  This code is based on various public domain sources.
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#	include "check.h"
#endif

#ifdef WORDS_BIGENDIAN
#	include "check_byteswap.h"
#endif


uint64_t lzma_crc64_table[4][256];


extern void
lzma_crc64_init(void)
{
	static const uint64_t poly64 = UINT64_C(0xC96C5795D7870F42);

	for (size_t s = 0; s < 4; ++s) {
		for (size_t b = 0; b < 256; ++b) {
			uint64_t r = s == 0 ? b : lzma_crc64_table[s - 1][b];

			for (size_t i = 0; i < 8; ++i) {
				if (r & 1)
					r = (r >> 1) ^ poly64;
				else
					r >>= 1;
			}

			lzma_crc64_table[s][b] = r;
		}
	}

#ifdef WORDS_BIGENDIAN
	for (size_t s = 0; s < 4; ++s)
		for (size_t b = 0; b < 256; ++b)
			lzma_crc64_table[s][b]
					= bswap_64(lzma_crc64_table[s][b]);
#endif

	return;
}
