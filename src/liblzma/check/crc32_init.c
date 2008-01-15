///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_init.c
/// \brief      CRC32 table initialization
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


uint32_t lzma_crc32_table[8][256];


extern void
lzma_crc32_init(void)
{
	static const uint32_t poly32 = UINT32_C(0xEDB88320);

	for (size_t s = 0; s < 8; ++s) {
		for (size_t b = 0; b < 256; ++b) {
			uint32_t r = s == 0 ? b : lzma_crc32_table[s - 1][b];

			for (size_t i = 0; i < 8; ++i) {
				if (r & 1)
					r = (r >> 1) ^ poly32;
				else
					r >>= 1;
			}

			lzma_crc32_table[s][b] = r;
		}
	}

#ifdef WORDS_BIGENDIAN
	for (size_t s = 0; s < 8; ++s)
		for (size_t b = 0; b < 256; ++b)
			lzma_crc32_table[s][b]
					= bswap_32(lzma_crc32_table[s][b]);
#endif

	return;
}
