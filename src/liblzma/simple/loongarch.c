// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       loongarch.c
/// \brief      Filter for loongarch binaries
///
/// This converts loongarch relative addresses in the BL and PCALAU12I
/// immediates to absolute values to increase redundancy of loongarch code.
///
/// Converting other instructions like JALR/B or PCADDI was tested but they
/// are not very useful, most of them is not used for functions calls, so
/// encoding them usually reduce the redundancy.
//
//  Authors:    Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"

static size_t
loongarch_code(void *simple_ptr, uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	if (size < 8)
		return 0;

	size -= 8;

	size_t i;

	for (i = 0; i < size; i += 4) {
		uint32_t pc = (uint32_t)(now_pos + i + 4);
		uint32_t instr = read32le(buffer + i + 4);

		if ((instr >> 26) == 0x15) {
			// BL instruction:
			// The full 26-bit immediate is converted.
			// The range is +/-128 MiB.
			//
			// Using the full range helps quite a lot with
			// big executables. Smaller range would reduce false
			// positives in non-code sections of the input though
			// so this is a compromise that slightly favors big
			// files. With the full range, only six bits of the 32
			// need to match to trigger a conversion.
			const uint32_t src = ((instr & 0x3FF) << 16)
					| ((instr & 0x3FFFFFF) >> 10);

			// We use previous instruction of the BL to reduce
			// false positives while filtering non-code buffer,
			// the previous instructions we use below are get
			// by analyze loongarch executables and libraries.
			uint32_t prev_instr = read32le(buffer + i);

			// The instructions below are (in opcode order):
			// 1. OR ~ LD.WU
			// 2. BEQZ, BNEZ
			// 3. B, BL
			// Only if prev_instr are in these instructions,
			// we'll convert BL's immediate.
			if ((prev_instr < 0x00150000 || prev_instr > 0x2A800000)
				&& (prev_instr & 0xF8000000) != 0x40000000
				&& (prev_instr & 0xF8000000) != 0x50000000)
				continue;

			instr = 0x54000000;

			pc >>= 2;
			if (!is_encoder)
				pc = 0U - pc;

			const uint32_t dest = src + pc;
			instr |= (dest & 0xFFFF) << 10;
			instr |= (dest & 0x3FF0000) >> 16;
			write32le(buffer + i + 4, instr);

		} else if ((instr >> 25) == 0xd) {
			// PCALAU12I instruction:
			// Only values in range +/-512MB are converted.
			//
			// Similar to ARM64's ADRP instruction, using
			// less than the full range reduces falses
			// positives on non-code sections of the input.
			const uint32_t src = ((instr >> 5) & 0xFFFFF);

			if ((src + 0x20000) & 0xC0000)
				continue;

			instr &= 0x1A00001F;

			pc >>= 12;
			if (!is_encoder)
				pc = 0U - pc;

			const uint32_t dest = src + pc;
			instr |= (dest & 0x3FFFF) << 5;
			instr |= (0U - (dest & 0x20000)) & 0x1800000;
			write32le(buffer + i + 4, instr);
		}
	}

	return i;
}


static lzma_ret
loongarch_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&loongarch_code, 0, 8, 4, is_encoder);
}


#ifdef HAVE_ENCODER_LOONGARCH
extern lzma_ret
lzma_simple_loongarch_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return loongarch_coder_init(next, allocator, filters, true);
}


extern LZMA_API(size_t)
lzma_bcj_loongarch_encode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	// start_offset must be a multiple of four.
	start_offset &= ~UINT32_C(3);
	return loongarch_code(NULL, start_offset, true, buf, size);
}
#endif


#ifdef HAVE_DECODER_LOONGARCH
extern lzma_ret
lzma_simple_loongarch_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return loongarch_coder_init(next, allocator, filters, false);
}


extern LZMA_API(size_t)
lzma_bcj_loongarch_decode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	// start_offset must be a multiple of four.
	start_offset &= ~UINT32_C(3);
	return loongarch_code(NULL, start_offset, false, buf, size);
}
#endif
