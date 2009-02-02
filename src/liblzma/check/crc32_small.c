///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_small.c
/// \brief      CRC32 calculation (size-optimized)
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"


uint32_t lzma_crc32_table[1][256];


static void
crc32_init(void)
{
	static const uint32_t poly32 = UINT32_C(0xEDB88320);

	for (size_t b = 0; b < 256; ++b) {
		uint32_t r = b;
		for (size_t i = 0; i < 8; ++i) {
			if (r & 1)
				r = (r >> 1) ^ poly32;
			else
				r >>= 1;
		}

		lzma_crc32_table[0][b] = r;
	}

	return;
}


extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
	mythread_once(crc32_init);

	crc = ~crc;

	while (size != 0) {
		crc = lzma_crc32_table[0][*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
