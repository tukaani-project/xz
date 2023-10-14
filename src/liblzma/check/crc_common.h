///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc_common.h
/// \brief      Some functions and macros for CRC32 and CRC64
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//              Hans Jansen
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifdef WORDS_BIGENDIAN
#	define A(x) ((x) >> 24)
#	define B(x) (((x) >> 16) & 0xFF)
#	define C(x) (((x) >> 8) & 0xFF)
#	define D(x) ((x) & 0xFF)

#	define S8(x) ((x) << 8)
#	define S32(x) ((x) << 32)

#else
#	define A(x) ((x) & 0xFF)
#	define B(x) (((x) >> 8) & 0xFF)
#	define C(x) (((x) >> 16) & 0xFF)
#	define D(x) ((x) >> 24)

#	define S8(x) ((x) >> 8)
#	define S32(x) ((x) >> 32)
#endif


#undef CRC_GENERIC
#undef CRC_CLMUL
#undef CRC_USE_IFUNC
#undef CRC_USE_GENERIC_FOR_SMALL_INPUTS

// If CLMUL cannot be used then only the generic slice-by-four is built.
#if !defined(HAVE_USABLE_CLMUL)
#	define CRC_GENERIC 1

// If CLMUL is allowed unconditionally in the compiler options then the
// generic version can be omitted. Note that this doesn't work with MSVC
// as I don't know how to detect the features here.
//
// NOTE: Keep this this in sync with crc32_table.c.
#elif (defined(__SSSE3__) && defined(__SSE4_1__) && defined(__PCLMUL__)) \
		|| (defined(__e2k__) && __iset__ >= 6)
#	define CRC_CLMUL 1

// Otherwise build both and detect at runtime which version to use.
#else
#	define CRC_GENERIC 1
#	define CRC_CLMUL 1

#	ifdef HAVE_FUNC_ATTRIBUTE_IFUNC
#		define CRC_USE_IFUNC 1
#	endif

/*
	// The generic code is much faster with 1-8-byte inputs and has
	// similar performance up to 16 bytes  at least in microbenchmarks
	// (it depends on input buffer alignment too). If both versions are
	// built, this #define will use the generic version for inputs up to
	// 16 bytes and CLMUL for bigger inputs. It saves a little in code
	// size since the special cases for 0-16-byte inputs will be omitted
	// from the CLMUL code.
#	define CRC_USE_GENERIC_FOR_SMALL_INPUTS 1
*/

#	if defined(_MSC_VER)
#		include <intrin.h>
#	elif defined(HAVE_CPUID_H)
#		include <cpuid.h>
#	endif
#endif

////////////////////////
// Detect CPU support //
////////////////////////

#if defined(CRC_GENERIC) && defined(CRC_CLMUL)
static inline bool
is_clmul_supported(void)
{
	int success = 1;
	uint32_t r[4]; // eax, ebx, ecx, edx

#if defined(_MSC_VER)
	// This needs <intrin.h> with MSVC. ICC has it as a built-in
	// on all platforms.
	__cpuid(r, 1);
#elif defined(HAVE_CPUID_H)
	// Compared to just using __asm__ to run CPUID, this also checks
	// that CPUID is supported and saves and restores ebx as that is
	// needed with GCC < 5 with position-independent code (PIC).
	success = __get_cpuid(1, &r[0], &r[1], &r[2], &r[3]);
#else
	// Just a fallback that shouldn't be needed.
	__asm__("cpuid\n\t"
			: "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
			: "a"(1), "c"(0));
#endif

	// Returns true if these are supported:
	// CLMUL (bit 1 in ecx)
	// SSSE3 (bit 9 in ecx)
	// SSE4.1 (bit 19 in ecx)
	const uint32_t ecx_mask = (1 << 1) | (1 << 9) | (1 << 19);
	return success && (r[2] & ecx_mask) == ecx_mask;

	// Alternative methods that weren't used:
	//   - ICC's _may_i_use_cpu_feature: the other methods should work too.
	//   - GCC >= 6 / Clang / ICX __builtin_cpu_supports("pclmul")
	//
	// CPUID decding is needed with MSVC anyway and older GCC. This keeps
	// the feature checks in the build system simpler too. The nice thing
	// about __builtin_cpu_supports would be that it generates very short
	// code as is it only reads a variable set at startup but a few bytes
	// doesn't matter here.
}
#endif


#define MASK_L(in, mask, r) r = _mm_shuffle_epi8(in, mask);
#define MASK_H(in, mask, r) \
	r = _mm_shuffle_epi8(in, _mm_xor_si128(mask, vsign));
#define MASK_LH(in, mask, low, high) \
	MASK_L(in, mask, low) MASK_H(in, mask, high)

#ifdef CRC_CLMUL

#include <immintrin.h>


#if (defined(__GNUC__) || defined(__clang__)) && !defined(__EDG__)
__attribute__((__target__("ssse3,sse4.1,pclmul")))
#endif
#if lzma_has_attribute(__no_sanitize_address__)
__attribute__((__no_sanitize_address__))
#endif
static inline void
crc_simd_body(const uint8_t *buf, size_t size, __m128i *v0, __m128i *v1,
		__m128i vfold16, __m128i crc2vec)
{
#if TUKLIB_GNUC_REQ(4, 6) || defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
	// Memory addresses A to D and the distances between them:
	//
	//     A           B     C         D
	//     [skip_start][size][skip_end]
	//     [     size2      ]
	//
	// A and D are 16-byte aligned. B and C are 1-byte aligned.
	// skip_start and skip_end are 0-15 bytes. size is at least 1 byte.
	//
	// A = aligned_buf will initially point to this address.
	// B = The address pointed by the caller-supplied buf.
	// C = buf + size == aligned_buf + size2
	// D = buf + size + skip_end == aligned_buf + size2 + skip_end
	uintptr_t skip_start = (uintptr_t)buf & 15;
	uintptr_t skip_end = -(uintptr_t)(buf + size) & 15;

	// Create a vector with 8-bit values 0 to 15.
	// This is used to construct control masks
	// for _mm_blendv_epi8 and _mm_shuffle_epi8.
	__m128i vramp = _mm_setr_epi32(
			0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c);

	// This is used to inverse the control mask of _mm_shuffle_epi8
	// so that bytes that wouldn't be picked with the original mask
	// will be picked and vice versa.
	__m128i vsign = _mm_set1_epi8(-0x80);

	// Masks to be used with _mm_blendv_epi8 and _mm_shuffle_epi8
	// The first skip_start or skip_end bytes in the vectors will hav
	// the high bit (0x80) set. _mm_blendv_epi8 and _mm_shuffle_epi
	// will produce zeros for these positions. (Bitwise-xor of thes
	// masks with vsign will produce the opposite behavior.)
	__m128i mask_start = _mm_sub_epi8(vramp, _mm_set1_epi8(skip_start));
	__m128i mask_end = _mm_sub_epi8(vramp, _mm_set1_epi8(skip_end));

	// If size2 <= 16 then the whole input fits into a single 16-byte
	// vector. If size2 > 16 then at least two 16-byte vectors must
	// be processed. If size2 > 16 && size <= 16 then there is only
	// one 16-byte vector's worth of input but it is unaligned in memory.
	//
	// NOTE: There is no integer overflow here if the arguments
	// are valid. If this overflowed, buf + size would too.
	uintptr_t size2 = skip_start + size;
	const __m128i *aligned_buf = (const __m128i*)((uintptr_t)buf & -16);
	__m128i v2, v3, vcrc, data0;

	vcrc = crc2vec;

	// Get the first 1-16 bytes into data0. If loading less than 16
	// bytes, the bytes are loaded to the high bits of the vector and
	// the least significant positions are filled with zeros.
	data0 = _mm_load_si128(aligned_buf);
	data0 = _mm_blendv_epi8(data0, _mm_setzero_si128(), mask_start);
	aligned_buf++;
	if (size2 <= 16) {
		//  There are 1-16 bytes of input and it is all
		//  in data0. Copy the input bytes to v3. If there
		//  are fewer than 16 bytes, the low bytes in v3
		//  will be filled with zeros. That is, the input
		//  bytes are stored to the same position as
		//  (part of) initial_crc is in v0.
		__m128i mask_low = _mm_add_epi8(
				vramp, _mm_set1_epi8(size - 16));
		MASK_LH(vcrc, mask_low, *v0, *v1)
		MASK_L(data0, mask_end, v3)
		*v0 = _mm_xor_si128(*v0, v3);
		*v1 = _mm_alignr_epi8(*v1, *v0, 8);
	} else {
		__m128i data1 = _mm_load_si128(aligned_buf);
		if (size <= 16) {
			//  Collect the 2-16 input bytes from data0 and data1
			//  to v2 and v3, and bitwise-xor them with the
			//  low bits of initial_crc in v0. Note that the
			//  the second xor is below this else-block as it
			//  is shared with the other branch.
			__m128i mask_low = _mm_add_epi8(
					vramp, _mm_set1_epi8(size - 16));
			MASK_LH(vcrc, mask_low, *v0, *v1);
			MASK_H(data0, mask_end, v2)
			MASK_L(data1, mask_end, v3)
			*v0 = _mm_xor_si128(*v0, v2);
			*v0 = _mm_xor_si128(*v0, v3);

			*v1 = _mm_alignr_epi8(*v1, *v0, 8);
		} else {
			const __m128i *end = (const __m128i*)(
					(char*)aligned_buf++ - 16 + size2);
			MASK_LH(vcrc, mask_start, *v0, *v1)
			*v0 = _mm_xor_si128(*v0, data0);
			*v1 = _mm_xor_si128(*v1, data1);
			while (aligned_buf < end) {
                                *v1 = _mm_xor_si128(*v1, _mm_clmulepi64_si128(*v0, vfold16, 0x00)); \
	                        *v0 = _mm_xor_si128(*v1, _mm_clmulepi64_si128(*v0, vfold16, 0x11));
                                *v1 = _mm_load_si128(aligned_buf++);
                        }

			if (aligned_buf != end) {
				MASK_H(*v0, mask_end, v2)
				MASK_L(*v0, mask_end, *v0)
				MASK_L(*v1, mask_end, v3)
				*v1 = _mm_or_si128(v2, v3);
			}
			*v1 = _mm_xor_si128(*v1, _mm_clmulepi64_si128(*v0, vfold16, 0x00));
	                *v0 = _mm_xor_si128(*v1, _mm_clmulepi64_si128(*v0, vfold16, 0x11));

			*v1 = _mm_srli_si128(*v0, 8);
		}
	}
}
#endif
