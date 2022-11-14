///////////////////////////////////////////////////////////////////////////////
//
/// \file       arm64.c
/// \brief      Filter for ARM64 binaries
///
/// This converts ARM64 relative addresses in the BL and ADRP immediates
/// to absolute values to increase redundancy of ARM64 code.
///
/// Unlike the older BCJ filters, this handles zeros specially. This way
/// the filter won't be counterproductive on Linux kernel modules, object
/// files, and static libraries where the immediates are all zeros (to be
/// filled later by a linker). Usually this has no downsides but with bad
/// luck it can reduce the effectiveness of the filter and trying a different
/// start offset can mitigate the problem.
///
/// Converting B or ADR instructions was also tested but it's not useful.
/// A majority of the jumps for the B instruction are very small (+/- 0xFF).
/// These are typical for loops and if-statements. Encoding them to their
/// absolute address reduces redundancy since many of the small relative
/// jump values are repeated, but very few of the absolute addresses are.
//
//  Authors:    Lasse Collin
//              Jia Tan
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"


static uint32_t
arm64_conv(uint32_t src, uint32_t pc, uint32_t mask, bool is_encoder)
{
	if (!is_encoder)
		pc = 0U - pc;

	uint32_t dest = src + pc;
	if ((dest & mask) == 0)
		dest = pc;

	return dest;
}


static size_t
arm64_code(void *simple lzma_attribute((__unused__)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	size_t i;

	// Clang 14.0.6 on x86-64 makes this four times bigger and 60 % slower
	// with auto-vectorization that is enabled by default with -O2.
	// Even -Os, which doesn't use vectorization, produces faster code.
	// Disabling vectorization with -O2 gives good speed (faster than -Os)
	// and reasonable code size.
	//
	// Such vectorization bloat happens with -O2 when targeting ARM64 too
	// but performance hasn't been tested.
	//
	// Clang 14 and 15 won't auto-vectorize this loop if the condition
	// for ADRP is replaced with the commented-out version. However,
	// at least Clang 14.0.6 doesn't generate as fast code with that
	// condition. The commented-out code is also bigger.
	//
	// GCC 12.2 on x86-64 with -O2 produces good code with both versions
	// of the ADRP if-statement although the single-branch version is
	// slightly faster and smaller than the commented-out version.
	// Speed is similar to non-vectorized clang -O2.
#ifdef __clang__
#	pragma clang loop vectorize(disable)
#endif
	for (i = 0; i + 4 <= size; i += 4) {
		const uint32_t pc = (uint32_t)(now_pos + i);
		uint32_t instr = read32le(buffer + i);

		if ((instr >> 26) == 0x25) {
			// BL instruction:
			// The full 26-bit immediate is converted.
			// The range is +/-128 MiB.
			//
			// Using the full range is helps quite a lot with
			// big executables. Smaller range would reduce false
			// positives in non-code sections of the input though
			// so this is a compromise that slightly favors big
			// files. With the full range only six bits of the 32
			// need to match to trigger a conversion.
			const uint32_t mask26 = 0x03FFFFFF;
			const uint32_t src = instr & mask26;
			instr = 0x94000000;

			if (src == 0)
				continue;

			instr |= arm64_conv(src, pc >> 2, mask26, is_encoder)
					& mask26;
			write32le(buffer + i, instr);

/*
		// This is a more readable version of the one below but this
		// has two branches. It results in bigger and slower code.
		} else if ((instr & 0x9FF00000) == 0x90000000
				|| (instr & 0x9FF00000) == 0x90F00000) {
*/
		// This is only a rotation, addition, and testing that
		// none of the bits covered by the bitmask are set.
		} else if (((((instr << 8) | (instr >> 24))
				+ (0x10000000 - 0x90)) & 0xE000009F) == 0) {
			// ADRP instruction:
			// Only values in the range +/-512 MiB are converted.
			//
			// Using less than the full +/-4 GiB range reduces
			// false positives on non-code sections of the input
			// while being excellent for executables up to 512 MiB.
			// The positive effect of ADRP conversion is smaller
			// than that of BL but it also doesn't hurt so much in
			// non-code sections of input because, with +/-512 MiB
			// range, nine bits of 32 need to match to trigger a
			// conversion (two 10-bit match choices = 9 bits).
			const uint32_t src = ((instr >> 29) & 3)
					| ((instr >> 3) & 0x0003FFFC);
			instr &= 0x9000001F;

			if (src == 0)
				continue;

			const uint32_t dest = arm64_conv(
					src, pc >> 12, 0x3FFFF, is_encoder);

			instr |= (dest & 3) << 29;
			instr |= (dest & 0x0003FFFC) << 3;
			instr |= (0U - (dest & 0x00020000)) & 0x00E00000;
			write32le(buffer + i, instr);
		}
	}

	return i;
}


static lzma_ret
arm64_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&arm64_code, 0, 4, 4, is_encoder);
}


#ifdef HAVE_ENCODER_ARM64
extern lzma_ret
lzma_simple_arm64_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return arm64_coder_init(next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER_ARM64
extern lzma_ret
lzma_simple_arm64_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return arm64_coder_init(next, allocator, filters, false);
}
#endif
