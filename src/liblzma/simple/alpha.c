// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       alpha.c
/// \brief      Filter for DEC Alpha binaries
///
/// This converts the displacements of BSR and of the GP displacement pairs
/// "ldah $gp,X($ra|$pv|$gp)" / "lda $gp,Y($gp)" to absolute values.
///
/// The base register of the "ldah" holds the address of the "ldah" itself:
/// $pv at a function entry, $ra after a "jsr", and $gp after the
/// "br $gp,.+4" that Clang emits. Thus the pair's 32-bit value
/// (X << 16) + sign_extend(Y) is ($gp - address_of_the_ldah).
///
/// BR mostly jumps within a function, so its displacement is converted
/// to be relative to the most recent pair with $pv or $gp as the base
/// register, which is usually the start of the current function.
///
/// Only "bsr $ra" and "br $zero" are converted. Compilers practically
/// never use other registers with them, and requiring the register
/// avoids most false positives in non-code data.
//
//  Author:     Matt Turner
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"


// The halves of a GP displacement pair may be up to four instructions apart.
#define ALPHA_LOOKAHEAD 20

#define ALPHA_RB(instr) (((instr) >> 16) & 0x1F)

#define ALPHA_REG_RA 26
#define ALPHA_REG_PV 27
#define ALPHA_REG_GP 29


static inline uint32_t
alpha_sign_extend_16(uint32_t value)
{
	return ((value & 0xFFFF) ^ 0x8000) - 0x8000;
}


typedef struct {
	/// Position of the most recent GP displacement pair whose "ldah"
	/// has $pv or $gp as the base register
	uint32_t func_pos;
} lzma_simple_alpha;


static size_t
alpha_code(void *simple_ptr, uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	if (size < ALPHA_LOOKAHEAD)
		return 0;

	lzma_simple_alpha *simple = simple_ptr;
	uint32_t func_pos = simple->func_pos;

	const size_t limit = size - ALPHA_LOOKAHEAD;
	size_t i = 0;

	while (i <= limit) {
		const uint32_t instr = read32le(buffer + i);

		// BR or BSR
		if ((instr & 0xEC000000) == 0xC0000000) {
			// The displacement is relative to the next instruction.
			// BSR becomes absolute and BR relative to func_pos.
			uint32_t pc = now_pos + (uint32_t)i + 4;

			if ((instr & 0xFFE00000) == 0xC3E00000) {
				// br $zero,disp
				pc -= func_pos;
			} else if ((instr & 0xFFE00000) != 0xD3400000) {
				// Not bsr $ra,disp
				i += 4;
				continue;
			}

			const uint32_t src = (instr & 0x001FFFFF) << 2;

			uint32_t dest;
			if (is_encoder)
				dest = pc + src;
			else
				dest = src - pc;

			dest >>= 2;

			write32le(buffer + i, (instr & 0xFFE00000)
					| (dest & 0x001FFFFF));

			i += 4;
			continue;
		}

		// ldah $gp,X($ra), ldah $gp,X($pv), or ldah $gp,X($gp)
		if ((instr & 0xFFE00000) != 0x27A00000
				|| (ALPHA_RB(instr) != ALPHA_REG_RA
					&& ALPHA_RB(instr) != ALPHA_REG_PV
					&& ALPHA_RB(instr) != ALPHA_REG_GP)) {
			i += 4;
			continue;
		}

		// lda $gp,Y($gp)
		size_t k = 4;
		uint32_t low = 0;

		for (; k < ALPHA_LOOKAHEAD; k += 4) {
			low = read32le(buffer + i + k);

			if ((low & 0xFFFF0000) == 0x23BD0000)
				break;
		}

		if (k == ALPHA_LOOKAHEAD) {
			i += 4;
			continue;
		}

		if (ALPHA_RB(instr) != ALPHA_REG_RA)
			func_pos = now_pos + (uint32_t)i;

		uint32_t value = (instr << 16) + alpha_sign_extend_16(low);

		if (is_encoder)
			value += now_pos + (uint32_t)i;
		else
			value -= now_pos + (uint32_t)i;

		const uint32_t new_low = value & 0xFFFF;
		const uint32_t new_high
			= (value - alpha_sign_extend_16(new_low)) >> 16;

		write32le(buffer + i, (instr & 0xFFFF0000)
				| (new_high & 0xFFFF));
		write32le(buffer + i + k, (low & 0xFFFF0000) | new_low);

		// Skip the "lda" so that no other "ldah" can claim it.
		i += k + 4;
	}

	simple->func_pos = func_pos;
	return i;
}


static lzma_ret
alpha_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	const lzma_ret ret = lzma_simple_coder_init(next, allocator, filters,
			&alpha_code, sizeof(lzma_simple_alpha),
			ALPHA_LOOKAHEAD, 4, is_encoder);

	if (ret == LZMA_OK) {
		lzma_simple_coder *coder = next->coder;
		lzma_simple_alpha *simple = coder->simple;
		simple->func_pos = 0;
	}

	return ret;
}


#ifdef HAVE_ENCODER_ALPHA
extern lzma_ret
lzma_simple_alpha_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return alpha_coder_init(next, allocator, filters, true);
}


extern LZMA_API(size_t)
lzma_bcj_alpha_encode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	// start_offset must be a multiple of four.
	start_offset &= ~UINT32_C(3);
	lzma_simple_alpha simple = { .func_pos = 0 };
	return alpha_code(&simple, start_offset, true, buf, size);
}
#endif


#ifdef HAVE_DECODER_ALPHA
extern lzma_ret
lzma_simple_alpha_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return alpha_coder_init(next, allocator, filters, false);
}


extern LZMA_API(size_t)
lzma_bcj_alpha_decode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	// start_offset must be a multiple of four.
	start_offset &= ~UINT32_C(3);
	lzma_simple_alpha simple = { .func_pos = 0 };
	return alpha_code(&simple, start_offset, false, buf, size);
}
#endif
