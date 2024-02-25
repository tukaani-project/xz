// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_loongarch.h
/// \brief      CRC32 calculation with LoongArch optimization
//
//  Authors:    Xi Ruoyao
//
///////////////////////////////////////////////////////////////////////////////


#ifndef LZMA_CRC32_LOONGARCH_H
#define LZMA_CRC32_LOONGARCH_H

#include <larchintrin.h>

static uint32_t
crc32_arch_optimized(const uint8_t *buf, size_t size, uint32_t crc_unsigned)
{
	int32_t crc = (int32_t)~crc_unsigned;

	// Align the input buffer because this was shown to be
	// significantly faster than unaligned accesses.
	const size_t align_amount = my_min(size,
			(8 - (uintptr_t)buf) & (8 - 1));

	if (align_amount & 1)
		crc = __crc_w_b_w((int8_t)*buf++, crc);

	if (align_amount & 2) {
		crc = __crc_w_h_w((int16_t)aligned_read16le(buf), crc);
		buf += 2;
	}

	if (align_amount & 4) {
		crc = __crc_w_w_w((int32_t)aligned_read32le(buf), crc);
		buf += 4;
	}

	size -= align_amount;

	// Process 8 bytes at a time. The end point is determined by
	// ignoring the least significant 3 bits of size to ensure
	// we do not process past the bounds of the buffer. This guarantees
	// that limit is a multiple of 8 and is strictly less than size.
	for (const uint8_t *limit = buf + (size & ~(size_t)7);
			buf < limit; buf += 8)
		crc = __crc_w_d_w((int64_t)aligned_read64le(buf), crc);

	// Process the remaining bytes that are not 8 byte aligned.
	const size_t remaining_amount = size & 7;

	if (remaining_amount & 4) {
		crc = __crc_w_w_w((int32_t)aligned_read32le(buf), crc);
		buf += 4;
	}

	if (remaining_amount & 2) {
		crc = __crc_w_h_w((int16_t)aligned_read16le(buf), crc);
		buf += 2;
	}

	if (remaining_amount & 1)
		crc = __crc_w_b_w((int8_t)*buf, crc);

	return (uint32_t)~crc;
}


#endif // LZMA_CRC32_LOONGARCH_H
