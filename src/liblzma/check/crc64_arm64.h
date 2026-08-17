// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_arm64.h
/// \brief      CRC64 calculation with ARM64 PMULL optimization
///
/// The CRC64 implementation uses 64-bit ARMv8 polynomial multiplication
/// (PMULL, vmull_p64) instructions.
///
/// See the Intel white paper "Fast CRC Computation for Generic Polynomials
/// Using PCLMULQDQ Instruction" from 2009:
/// https://www.researchgate.net/publication/263424619_Fast_CRC_computation
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_CRC64_ARM64_H
#define LZMA_CRC64_ARM64_H

#include <arm_neon.h>

#if defined(CRC64_GENERIC) && defined(CRC64_ARCH_OPTIMIZED)
#	if (defined(HAVE_GETAUXVAL) && defined(HAVE_HWCAP_PMULL)) \
			|| defined(HAVE_ELF_AUX_INFO)
#		include <sys/auxv.h>
#	elif defined(_WIN32)
#		include <processthreadsapi.h>
#	elif defined(__APPLE__) && defined(HAVE_SYSCTLBYNAME)
#		include <sys/sysctl.h>
#	endif
#endif

// Some EDG-based compilers support ARM64 and define __GNUC__
// but do not support function attributes.
//
// NOTE: Build systems check for this too, keep them in sync with this.
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__EDG__)
#	define crc64_attr_target __attribute__((__target__("+crypto")))
#else
#	define crc64_attr_target
#endif


// Align it so that the whole array is within the same cache line.
// More than one unaligned load can be done from this during the
// same CRC function call.
//
// The bytes [0] to [31] are used with AND to clear the low bytes.
// The bytes [16] to [47] are for left shifts.
// The bytes [32] to [63] are for right shifts.
alignas(64)
static const uint8_t vmasks[64] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};


// Clear the lowest (16 - count) bytes and keep the highest "count" bytes.
static inline uint8x16_t
keep_high_bytes(uint8x16_t v, size_t count)
{
	return vandq_u8(v, vld1q_u8(vmasks + count));
}


// Shift left by "amount" bytes. The lowest "amount" bytes are cleared.
static inline uint8x16_t
shift_left(uint8x16_t v, size_t amount)
{
	return vqtbl1q_u8(v, vld1q_u8(vmasks + 32 - amount));
}


// Shift right by "amount" bytes. The highest "amount" bytes are cleared.
static inline uint8x16_t
shift_right(uint8x16_t v, size_t amount)
{
	return vqtbl1q_u8(v, vld1q_u8(vmasks + 32 + amount));
}


static inline uint8x16_t
clmul_00(uint8x16_t a, uint8x16_t b)
{
	uint64x2_t a64 = vreinterpretq_u64_u8(a);
	uint64x2_t b64 = vreinterpretq_u64_u8(b);
	return (uint8x16_t)vmull_p64((poly64_t)vgetq_lane_u64(a64, 0),
			(poly64_t)vgetq_lane_u64(b64, 0));
}


static inline uint8x16_t
clmul_10(uint8x16_t a, uint8x16_t b)
{
	uint64x2_t a64 = vreinterpretq_u64_u8(a);
	uint64x2_t b64 = vreinterpretq_u64_u8(b);
	return (uint8x16_t)vmull_p64((poly64_t)vgetq_lane_u64(a64, 0),
			(poly64_t)vgetq_lane_u64(b64, 1));
}


static inline uint8x16_t
clmul_11(uint8x16_t a, uint8x16_t b)
{
	uint64x2_t a64 = vreinterpretq_u64_u8(a);
	uint64x2_t b64 = vreinterpretq_u64_u8(b);
	return (uint8x16_t)vmull_p64((poly64_t)vgetq_lane_u64(a64, 1),
			(poly64_t)vgetq_lane_u64(b64, 1));
}


static inline uint8x16_t
fold(uint8x16_t v, uint8x16_t fold_const)
{
	uint8x16_t p0 = clmul_00(v, fold_const);
	uint8x16_t p1 = clmul_11(v, fold_const);
	return veorq_u8(p0, p1);
}


static inline uint8x16_t
fold_xor(uint8x16_t v, uint8x16_t fold_const, const uint8_t *p)
{
	return veorq_u8(fold(v, fold_const), vld1q_u8(p));
}


crc64_attr_target
static uint64_t
crc64_arch_optimized(const uint8_t *buf, size_t size, uint64_t crc)
{
	// We will assume that there is at least one byte of input.
	if (size == 0)
		return crc;

	// See crc_clmul_consts_gen.c.
	const uint8x16_t fold512 = (uint8x16_t)vcombine_u64(
			vcreate_u64(0x6ae3efbb9dd441f3ULL),
			vcreate_u64(0x081f6054a7842df4ULL));
	const uint8x16_t fold128 = (uint8x16_t)vcombine_u64(
			vcreate_u64(0xe05dd497ca393ae4ULL),
			vcreate_u64(0xdabe95afc7875f40ULL));
	const uint8x16_t mu_p = (uint8x16_t)vcombine_u64(
			vcreate_u64(0x92d8af2baf0e1e84ULL),
			vcreate_u64(0x9c3e466c172963d5ULL));

	uint8x16_t v0, v1, v2, v3;

	crc = ~crc;

	if (size < 8) {
		uint64_t x = crc;
		size_t i = 0;

		// Checking the bit instead of comparing the size means
		// that we don't need to update the size between the steps.
		if (size & 4) {
			x ^= read32le(buf);
			buf += 4;
			i = 32;
		}

		if (size & 2) {
			x ^= (uint64_t)read16le(buf) << i;
			buf += 2;
			i += 16;
		}

		if (size & 1)
			x ^= (uint64_t)*buf << i;

		v0 = (uint8x16_t)vsetq_lane_u64(x, vdupq_n_u64(0), 0);
		v0 = shift_left(v0, 8 - size);

	} else if (size < 16) {
		v0 = (uint8x16_t)vsetq_lane_u64(crc ^ read64le(buf),
				vdupq_n_u64(0), 0);

		// NOTE: buf is intentionally left 8 bytes behind so that
		// we can read the last 1-7 bytes with read64le(buf + size).
		size -= 8;

		// Handling 8-byte input specially is a speed optimization
		// as the clmul can be skipped. A branch is also needed to
		// avoid a too high shift amount.
		if (size > 0) {
			const size_t padding = 8 - size;
			uint64_t high = read64le(buf + size) >> (padding * 8);

			v0 = (uint8x16_t)vsetq_lane_u64(high,
					vreinterpretq_u64_u8(v0), 1);
			v0 = shift_left(v0, padding);

			v1 = vextq_u8(v0, vdupq_n_u8(0), 8);
			v0 = clmul_10(v0, fold128);
			v0 = veorq_u8(v0, v1);
		}
	} else {
		v0 = (uint8x16_t)vsetq_lane_u64(crc, vdupq_n_u64(0), 0);
		v0 = veorq_u8(v0, vld1q_u8(buf));
		buf += 16;
		size -= 16;

		if (size >= 48) {
			v1 = vld1q_u8(buf);
			v2 = vld1q_u8(buf + 16);
			v3 = vld1q_u8(buf + 32);
			buf += 48;
			size -= 48;

			while (size >= 64) {
				v0 = fold_xor(v0, fold512, buf);
				v1 = fold_xor(v1, fold512, buf + 16);
				v2 = fold_xor(v2, fold512, buf + 32);
				v3 = fold_xor(v3, fold512, buf + 48);
				buf += 64;
				size -= 64;
			}

			v0 = veorq_u8(v1, fold(v0, fold128));
			v0 = veorq_u8(v2, fold(v0, fold128));
			v0 = veorq_u8(v3, fold(v0, fold128));
		}

		while (size >= 16) {
			v0 = fold_xor(v0, fold128, buf);
			buf += 16;
			size -= 16;
		}

		if (size > 0) {
			// We want the last "size" number of input bytes to
			// be at the high bits of v1. First do a full 16-byte
			// unaligned load from the end of the input buffer.
			v1 = vld1q_u8(buf + size - 16);

			// Clear the lowest (16 - size) bytes and keep the highest
			// "size" bytes.
			v1 = keep_high_bytes(v1, size);

			// Shift the already processed bytes in v0 so that
			// the high bytes (which are from the end of the
			// previously loaded block) are shifted to the low
			// bits. Combine the low and high bytes to get the
			// full 16-byte vector.
			v1 = vorrq_u8(v1, shift_right(v0, size));
			v0 = shift_left(v0, 16 - size);
			v0 = veorq_u8(v1, fold(v0, fold128));
		}

		v1 = vextq_u8(v0, vdupq_n_u8(0), 8);
		v0 = clmul_10(v0, fold128);
		v0 = veorq_u8(v0, v1);
	}

	// Barrett reduction
	v1 = clmul_10(v0, mu_p);
	v2 = vextq_u8(vdupq_n_u8(0), v1, 8);
	v1 = clmul_00(v1, mu_p);
	v0 = veorq_u8(v0, v2);
	v0 = veorq_u8(v0, v1);

	return ~vgetq_lane_u64(vreinterpretq_u64_u8(v0), 1);
}


#if defined(CRC64_GENERIC) && defined(CRC64_ARCH_OPTIMIZED)
static inline bool
is_arch_extension_supported(void)
{
#if defined(HAVE_GETAUXVAL) && defined(HAVE_HWCAP_PMULL)
	return (getauxval(AT_HWCAP) & HWCAP_PMULL) != 0;

#elif defined(HAVE_ELF_AUX_INFO)
	unsigned long feature_flags;

	if (elf_aux_info(AT_HWCAP, &feature_flags, sizeof(feature_flags)) != 0)
		return false;

	return (feature_flags & HWCAP_PMULL) != 0;

#elif defined(_WIN32)
	return IsProcessorFeaturePresent(
			PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);

#elif defined(__APPLE__) && defined(HAVE_SYSCTLBYNAME)
	int has_pmull = 0;
	size_t size = sizeof(has_pmull);

	if (sysctlbyname("hw.optional.arm.FEAT_PMULL", &has_pmull,
			&size, NULL, 0) != 0)
		return false;

	return has_pmull != 0;

#else
	// If a runtime detection method cannot be found, then this must
	// be a compile time error. The checks in crc_common.h should ensure
	// a runtime detection method is always found if this function is
	// built.
#	error Runtime detection method unavailable.
#endif
}
#endif

#endif // LZMA_CRC64_ARM64_H
