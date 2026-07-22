// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_rvv.h
/// \brief      CRC32 calculation with RISC-V Vector optimization
//
//  Author:     Ding Chenguang
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_CRC32_RVV_H
#define LZMA_CRC32_RVV_H

#ifdef __riscv_vector
#	include <riscv_vector.h>

/// \brief      Get the maximum vector length in bytes (8-bit elements)
#	define RVV_VL_MAX_BYTES() __riscv_vsetvlmax_e8m8()

/// \brief      Get the maximum vector length in 32-bit words
#	define RVV_VL_MAX_WORDS() __riscv_vsetvlmax_e32m8()

/// \brief      Check if a pointer is 8-byte aligned
#	define RVV_IS_ALIGNED_8(ptr) (((uintptr_t)(ptr) & 7) == 0)

/// \brief      Minimum buffer size in bytes to benefit from vectorization
#	define RVV_MIN_SIZE_FOR_VECTOR 32

/// \brief      Recommended processing block size in bytes
#	define RVV_BLOCK_SIZE 64

/// \brief      Get the vector length, limited by remaining bytes and max_vl
#	define RVV_GET_VL(remaining, max_vl) \
	__riscv_vsetvl_e8m8(((remaining) < (max_vl)) \
		? (size_t)(remaining) : (size_t)(max_vl))
#endif


static uint32_t
crc32_arch_optimized(const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = byteswap32(crc);
#endif

	// For very small buffers, use a simple byte-by-byte approach
	// to avoid the overhead of vector setup.
	if (size < RVV_MIN_SIZE_FOR_VECTOR) {
		while (size-- != 0)
			crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
		crc = byteswap32(crc);
#endif
		return ~crc;
	}

	// Get the maximum vector length and cap it to the block size.
	const size_t vlmax = RVV_VL_MAX_BYTES();
	const size_t actual_vl = vlmax < RVV_BLOCK_SIZE
			? vlmax : RVV_BLOCK_SIZE;

	// Align the buffer to an 8-byte boundary.
	while (size > 0 && !RVV_IS_ALIGNED_8(buf)) {
		crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);
		--size;
	}

	// Vectorized main loop: load data into vector registers, then
	// process each byte through the CRC32 lookup table.
	while (size >= actual_vl) {
		const size_t vl = RVV_GET_VL(size, actual_vl);

		vuint8m8_t vec_data = __riscv_vle8_v_u8m8(buf, vl);

		uint8_t temp[RVV_BLOCK_SIZE];
		__riscv_vse8_v_u8m8(temp, vec_data, vl);

		for (size_t i = 0; i < vl; ++i)
			crc = lzma_crc32_table[0][temp[i] ^ A(crc)] ^ S8(crc);

		buf += vl;
		size -= vl;
	}

	// Process remaining 0-7 bytes.
	while (size-- != 0)
		crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
	crc = byteswap32(crc);
#endif

	return ~crc;
}


#if defined(CRC32_GENERIC) && defined(CRC32_ARCH_OPTIMIZED)
static inline bool
is_arch_extension_supported(void)
{
	// When __riscv_vector is defined, the code has been compiled with
	// RISC-V Vector extension support. On RISC-V Linux 6.4+, this could
	// use riscv_hwprobe() for proper runtime detection.
	//
	// For now we return true because this header is only included when
	// the build system has confirmed RVV support is available.
	return true;
}
#endif

#endif // LZMA_CRC32_RVV_H
