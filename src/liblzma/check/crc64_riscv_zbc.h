// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_riscv_zbc.h
/// \brief      CRC64 calculation using RISC-V Zbc carry-less multiplication
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_CRC64_RISCV_ZBC_H
#define LZMA_CRC64_RISCV_ZBC_H

#if !(defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64)
#	error crc64_riscv_zbc.h requires RV64
#endif

// If both implementations are built, runtime detection uses Linux
// riscv_hwprobe. crc_common.h only enables this combination when
// HAVE_RISCV_HWPROBE is available.
#if defined(CRC64_GENERIC) && defined(CRC64_ARCH_OPTIMIZED)
#	include <asm/hwprobe.h>
#	include <sys/syscall.h>
#	include <unistd.h>
#endif


typedef struct {
	uint64_t lo;
	uint64_t hi;
} crc64_zbc_u128;


static inline crc64_zbc_u128
crc64_zbc_xor128(crc64_zbc_u128 a, crc64_zbc_u128 b)
{
	return (crc64_zbc_u128){ a.lo ^ b.lo, a.hi ^ b.hi };
}


static inline crc64_zbc_u128
crc64_zbc_or128(crc64_zbc_u128 a, crc64_zbc_u128 b)
{
	return (crc64_zbc_u128){ a.lo | b.lo, a.hi | b.hi };
}


static inline crc64_zbc_u128
crc64_zbc_load128(const uint8_t *p)
{
	return (crc64_zbc_u128){ read64le(p), read64le(p + 8) };
}


// Shift the 128-bit value left by "amount" bytes (not bits).
static inline crc64_zbc_u128
crc64_zbc_shift_left(crc64_zbc_u128 v, size_t amount)
{
	if (amount == 0)
		return v;

	if (amount < 8) {
		const unsigned shift = (unsigned)amount * 8;
		return (crc64_zbc_u128){
			v.lo << shift,
			(v.hi << shift) | (v.lo >> (64 - shift))
		};
	}

	if (amount == 8)
		return (crc64_zbc_u128){ 0, v.lo };

	if (amount < 16) {
		const unsigned shift = (unsigned)(amount - 8) * 8;
		return (crc64_zbc_u128){ 0, v.lo << shift };
	}

	return (crc64_zbc_u128){ 0, 0 };
}


// Shift the 128-bit value right by "amount" bytes (not bits).
static inline crc64_zbc_u128
crc64_zbc_shift_right(crc64_zbc_u128 v, size_t amount)
{
	if (amount == 0)
		return v;

	if (amount < 8) {
		const unsigned shift = (unsigned)amount * 8;
		return (crc64_zbc_u128){
			(v.lo >> shift) | (v.hi << (64 - shift)),
			v.hi >> shift
		};
	}

	if (amount == 8)
		return (crc64_zbc_u128){ v.hi, 0 };

	if (amount < 16) {
		const unsigned shift = (unsigned)(amount - 8) * 8;
		return (crc64_zbc_u128){ v.hi >> shift, 0 };
	}

	return (crc64_zbc_u128){ 0, 0 };
}


// Keep the highest "count" bytes as is and clear the remaining low bytes.
static inline crc64_zbc_u128
crc64_zbc_keep_high_bytes(crc64_zbc_u128 v, size_t count)
{
	const size_t clear = 16 - count;
	return crc64_zbc_shift_left(crc64_zbc_shift_right(v, clear), clear);
}


// Full 64 x 64 -> 128 carry-less multiplication.
//
// .option arch,+zbc enables Zbc only for these instructions. Unlike a
// function target attribute, this doesn't raise the ELF minimum ISA
// requirement when the rest of the translation unit is built for rv64gc.
static inline crc64_zbc_u128
crc64_zbc_clmul64(uint64_t a, uint64_t b)
{
	crc64_zbc_u128 r;

	__asm__(
		".option push\n\t"
		".option arch, +zbc\n\t"
		"clmul  %0, %2, %3\n\t"
		"clmulh %1, %2, %3\n\t"
		".option pop"
		: "=&r"(r.lo), "=&r"(r.hi)
		: "r"(a), "r"(b));

	return r;
}


static inline crc64_zbc_u128
crc64_zbc_fold(crc64_zbc_u128 v, crc64_zbc_u128 k)
{
	return crc64_zbc_xor128(
			crc64_zbc_clmul64(v.lo, k.lo),
			crc64_zbc_clmul64(v.hi, k.hi));
}


static inline crc64_zbc_u128
crc64_zbc_fold_xor(
		crc64_zbc_u128 v, crc64_zbc_u128 k, const uint8_t *buf)
{
	return crc64_zbc_xor128(
			crc64_zbc_load128(buf), crc64_zbc_fold(v, k));
}


static uint64_t
crc64_arch_optimized(const uint8_t *buf, size_t size, uint64_t crc)
{
	if (size == 0)
		return crc;

	// See crc_clmul_consts_gen.c.
	// _mm_set_epi64x(high, low) maps to { low, high } here.
	const crc64_zbc_u128 fold512 = {
		UINT64_C(0x6ae3efbb9dd441f3),
		UINT64_C(0x081f6054a7842df4)
	};

	const crc64_zbc_u128 fold128 = {
		UINT64_C(0xe05dd497ca393ae4),
		UINT64_C(0xdabe95afc7875f40)
	};

	const crc64_zbc_u128 mu_p = {
		UINT64_C(0x92d8af2baf0e1e84),
		UINT64_C(0x9c3e466c172963d5)
	};

	crc64_zbc_u128 v0, v1, v2, v3;

	crc = ~crc;

	if (size < 8) {
		uint64_t x = crc;
		size_t i = 0;

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

		v0 = (crc64_zbc_u128){ x, 0 };
		v0 = crc64_zbc_shift_left(v0, 8 - size);

	} else if (size < 16) {
		v0 = (crc64_zbc_u128){ crc ^ read64le(buf), 0 };

		// NOTE: buf is intentionally left 8 bytes behind so that
		// we can read the last 1-7 bytes with read64le(buf + size).
		size -= 8;

		if (size > 0) {
			const size_t padding = 8 - size;
			const uint64_t high = read64le(buf + size)
					>> (padding * 8);

			v0.hi = high;
			v0 = crc64_zbc_shift_left(v0, padding);

			v1 = crc64_zbc_shift_right(v0, 8);

			// x86 PCLMUL selector 0x10: v0.lo * fold128.hi.
			v0 = crc64_zbc_clmul64(v0.lo, fold128.hi);
			v0 = crc64_zbc_xor128(v0, v1);
		}

	} else {
		v0 = (crc64_zbc_u128){ crc, 0 };

		v0 = crc64_zbc_xor128(v0, crc64_zbc_load128(buf));
		buf += 16;
		size -= 16;

		if (size >= 48) {
			v1 = crc64_zbc_load128(buf);
			v2 = crc64_zbc_load128(buf + 16);
			v3 = crc64_zbc_load128(buf + 32);
			buf += 48;
			size -= 48;

			while (size >= 64) {
				v0 = crc64_zbc_fold_xor(v0, fold512, buf);
				v1 = crc64_zbc_fold_xor(v1, fold512, buf + 16);
				v2 = crc64_zbc_fold_xor(v2, fold512, buf + 32);
				v3 = crc64_zbc_fold_xor(v3, fold512, buf + 48);
				buf += 64;
				size -= 64;
			}

			v0 = crc64_zbc_xor128(
					v1, crc64_zbc_fold(v0, fold128));
			v0 = crc64_zbc_xor128(
					v2, crc64_zbc_fold(v0, fold128));
			v0 = crc64_zbc_xor128(
					v3, crc64_zbc_fold(v0, fold128));
		}

		while (size >= 16) {
			v0 = crc64_zbc_fold_xor(v0, fold128, buf);
			buf += 16;
			size -= 16;
		}

		if (size > 0) {
			v1 = crc64_zbc_load128(buf + size - 16);
			v1 = crc64_zbc_keep_high_bytes(v1, size);

			v1 = crc64_zbc_or128(
					v1, crc64_zbc_shift_right(v0, size));
			v0 = crc64_zbc_shift_left(v0, 16 - size);
			v0 = crc64_zbc_xor128(
					v1, crc64_zbc_fold(v0, fold128));
		}

		v1 = crc64_zbc_shift_right(v0, 8);

		// x86 PCLMUL selector 0x10: CLMUL(v0.lo, fold128.hi).
		v0 = crc64_zbc_xor128(
				crc64_zbc_clmul64(v0.lo, fold128.hi), v1);
	}

	// Barrett reduction. This is the same operation sequence as the
	// CRC64 branch in crc_x86_clmul.h.
	v1 = crc64_zbc_clmul64(v0.lo, mu_p.hi); // v0 * mu

	// p is 65 bits. Compensate for its implicit highest bit.
	v2 = (crc64_zbc_u128){ 0, v1.lo };

	v1 = crc64_zbc_clmul64(v1.lo, mu_p.lo); // v1 * p'
	v0 = crc64_zbc_xor128(v0, v2);
	v0 = crc64_zbc_xor128(v0, v1);

	return ~v0.hi;
}


#if defined(CRC64_GENERIC) && defined(CRC64_ARCH_OPTIMIZED)
static inline bool
is_arch_extension_supported(void)
{
	struct riscv_hwprobe pair = {
		.key = RISCV_HWPROBE_KEY_IMA_EXT_0,
		.value = 0,
	};

#if defined(SYS_riscv_hwprobe)
	const long ret = syscall(SYS_riscv_hwprobe,
			&pair, 1, 0, NULL, 0);
#elif defined(__NR_riscv_hwprobe)
	const long ret = syscall(__NR_riscv_hwprobe,
			&pair, 1, 0, NULL, 0);
#else
#	error RISC-V hwprobe syscall number unavailable
#endif

	return ret == 0
			&& pair.key == RISCV_HWPROBE_KEY_IMA_EXT_0
			&& (pair.value & RISCV_HWPROBE_EXT_ZBC) != 0;
}
#endif


#endif // LZMA_CRC64_RISCV_ZBC_H
