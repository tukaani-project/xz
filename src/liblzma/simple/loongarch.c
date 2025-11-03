// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       loongarch.c
/// \brief      Filter for loongarch binaries
///
/// This converts loongarch relative addresses in the BL, PCALAU12I. PCADDU18I
/// immediates to absolute values to increase redundancy of loongarch code.
///
/// There are many other PC-relative instrutions, including PCADDI, PCADDU12I
/// AND B, converting them was tested but they are not very useful, most of
/// them is not used for functions calls, even if they do, their range are
/// usually too small so encoding them usually reduce the redundancy.
//
//  Authors:    Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"

// This macro checks multiple conditions:
// (1) inst2 is JIRL
// (2) JIRL's rj equals to PCADDU18I's rd
// (3) JIRL's rd = $ra/$zero
// These conditions could be found in laelf.adoc
// we use them to further reduce false-positives.
#define NOT_PCADDU18I_PAIR(pcaddu18i, jirl) \
	(((jirl >> 26) != 0x13) \
	|| ((pcaddu18i & 0x1F) != ((jirl >> 5) & 0x1F)) \
	|| ((jirl & 0x1F) != 0x1 && (jirl & 0x1F) != 0x0))

static size_t
loongarch_code(void *simple_ptr, uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	if (size < 12)
		return 0;

	size -= 12;

	size_t i;

	for (i = 0; i <= size; i += 4) {
		uint32_t pc = (uint32_t)(now_pos + i + 4);
		uint32_t inst = read32le(buffer + i + 4);

		if ((inst >> 26) == 0x15) {
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
			const uint32_t src = ((inst & 0x3FF) << 16)
					| ((inst & 0x3FFFFFF) >> 10);

			// We use previous instruction of the BL to reduce
			// false positives while filtering non-code buffer,
			// the previous instructions we use below are get
			// by analyze loongarch executables and libraries.
			uint32_t prev_inst = read32le(buffer + i);

			// The instructions below are (in opcode order):
			// 1. OR ~ LD.WU
			// 2. BEQZ, BNEZ, B, BL
			// Only if prev_inst are in these instructions,
			// we'll convert BL's immediate.
			if ((prev_inst - 0x00150000) > 0x2A800000 - 0x00150000
				&& (prev_inst & 0xE8000000) != 0x40000000)
				continue;

			inst = 0x54000000;

			pc >>= 2;
			if (!is_encoder)
				pc = 0U - pc;

			const uint32_t dest = src + pc;
			inst |= (dest & 0xFFFF) << 10;
			inst |= (dest & 0x3FF0000) >> 16;
			write32le(buffer + i + 4, inst);

		} else if ((inst >> 25) == 0xC) {
			// PCADDI instruction:
			// We use the previous instruction again to avoid
			// false-positives.
			const uint32_t src = (inst >> 5) & 0xFFFFF;
			inst &= 0x1800001F;

			uint32_t prev_inst = read32le(buffer + i);

			if ((prev_inst - 0x00150000) > 0x2A800000 - 0x00150000
				&& (prev_inst & 0xE8000000) != 0x40000000)
				continue;

			pc >>= 2;
			if (!is_encoder)
				pc = 0U - pc;

			const uint32_t dest = src + pc;
			inst |= (dest & 0xFFFFF) << 5;
			inst |= (0U - (dest & 0x80000)) & 0x1000000;
			write32le(buffer + i + 4, inst);

		} else if ((inst >> 25) == 0xD) {
			// PCALAU12I instruction:
			// Only values in range +/-512MB are converted.
			//
			// Similar to ARM64's ADRP instruction, using
			// less than the full range reduces falses
			// positives on non-code sections of the input.
			const uint32_t src = ((inst >> 5) & 0xFFFFF);

			if ((src + 0x20000) & 0xC0000)
				continue;

			inst &= 0x1A00001F;

			pc >>= 12;
			if (!is_encoder)
				pc = 0U - pc;

			const uint32_t dest = src + pc;
			inst |= (dest & 0x3FFFF) << 5;
			inst |= (0U - (dest & 0x20000)) & 0x1800000;
			write32le(buffer + i + 4, inst);

		} else if ((inst >> 25) == 0xF) {
			// PCADDU18I + JIRL pair
			// According to laelf.adoc and elfnn-loongarch.c in
			// binutils this instruction pair is used for medium
			// code model only, so detect them as a whole to reduce
			// false-positives. And because the original PCADDU18I
			// shift the immediate by 18-bit and sign-extend, so we
			// have to use uint64_t to store address.
			//
			// Also, due to all loongarch instrcutions are 4 bytes
			// long, we could just shift the pc by 2 bits instead
			// of shift JIRL or PCADDU18I's immediate by 18 or
			// 2 bits, so that the whole address would fit into
			// 36 bits instead of 38 bits.
			uint32_t inst2 = read32le(buffer + i + 8);
			uint64_t addr = 0;

			if (is_encoder) {
				addr = (((uint64_t)inst >> 5) & 0xFFFFF) << 16;
				if (addr & 0x800000000)
					addr |= 0xFFFFFFF000000000;

				if (NOT_PCADDU18I_PAIR(inst, inst2))
					continue;

				uint64_t inst2_addr =
					((uint64_t)inst2 >> 10 & 0xFFFF);

				// Sign-extend the 16-bit immediate in JIRL.
				if (inst2_addr & 0x8000)
					inst2_addr |= 0xFFFFFFFFFFFF0000;

				addr += inst2_addr;

				// Skipping if addr equals 0 to improve
				// compress ratio of .o and .a files.
				if (addr == 0)
					continue;

				if (addr + (pc >> 2) == 0)
					addr = 0;

				addr += pc >> 2;

				inst &= 0x1E00001F;
				inst |= (addr & 0xFFFFF) << 5;

				inst2 &= 0xFC0003FF;
				inst2 |= ((addr >> 20) & 0xFFFF) << 10;

				write32le(buffer + i + 4, inst);
				write32le(buffer + i + 8, inst2);

			} else {
				if (NOT_PCADDU18I_PAIR(inst, inst2))
					continue;

				addr = ((uint64_t)inst >> 5) & 0xFFFFF |
					((uint64_t)inst2 >> 10 & 0xFFFF) << 20;

				if (addr & 0x800000000)
					addr |= 0xFFFFFFF000000000;

				if (addr == 0)
					continue;

				if (addr == pc >> 2)
					addr = 0;

				addr -= pc >> 2;

				inst &= 0x1E00001F;
				inst |= (addr + 0x8000 >> 16 << 5) & 0x1FFFFE0;

				inst2 &= 0xFC0003FF;
				inst2 |= (addr & 0x3FFFF) << 10;

				write32le(buffer + i + 4, inst);
				write32le(buffer + i + 8, inst2);
			}
		}
	}

	return i;
}


static lzma_ret
loongarch_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&loongarch_code, 0, 12, 4, is_encoder);
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
