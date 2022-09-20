///////////////////////////////////////////////////////////////////////////////
//
/// \file       arm64.c
/// \brief      Filter for ARM64 binaries
///
//  Authors:    Lasse Collin
//              Jia Tan
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"

#ifdef HAVE_ENCODER_ARM64
#	include "simple_encoder.h"
#endif

#ifdef HAVE_DECODER_ARM64
#	include "simple_decoder.h"
#endif


// In ARM64, there are two main branch instructions.
// bl - branch and link: Calls a function and stores the return address.
// b - branch: Jumps to a location, but does not store a return address.
//
// After some benchmarking, it was determined that only the bl instruction
// is beneficial for compression. A majority of the jumps for the b
// instruction are very small (+/- 0xFF). These are typical for loops
// and if-statements. Encoding them to their absolute address reduces
// redundancy since many of the small relative jump values are repeated,
// but very few of the absolute addresses are.
//
// Thus, only the bl instruction will be encoded and decoded.
// The bl instruction is 32 bits in size. The highest 6 bits contain
// the opcode (10 0101 == 0x25) and the remaining 26 bits are
// the immediate value. The immediate is a signed integer that
// encodes the target address as a multiple of four bytes so
// the range is +/-128 MiB.

// The 6-bit op code for the bl instruction in ARM64
#define ARM64_BL_OPCODE 0x25

// Once the 26-bit immediate is multiple by four, the address is 28 bits
// with the two lowest bits being zero. This mask is used to clear the
// unwanted bits.
#define ADDR28_MASK 0x0FFFFFFCU


typedef struct {
	uint32_t sign_bit;
	uint32_t sign_mask;
} lzma_simple_arm64;


static size_t
arm64_code(void *simple_ptr, uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	const lzma_simple_arm64 *simple = simple_ptr;
	const uint32_t sign_bit = simple->sign_bit;
	const uint32_t sign_mask = simple->sign_mask;

	size_t i;
	for (i = 0; i + 4 <= size; i += 4) {
		if ((buffer[i + 3] >> 2) == ARM64_BL_OPCODE) {
			// Get the relative 28-bit address from
			// the 26-bit immediate.
			uint32_t src = read32le(buffer + i);
			src <<= 2;
			src &= ADDR28_MASK;

			// When the conversion width isn't the maximum,
			// check that the highest bits are either all zero
			// or all one.
			if ((src & sign_mask) != 0
					&& (src & sign_mask) != sign_mask)
				continue;

			// Some files like static libraries or Linux kernel
			// modules have the immediate value filled with
			// zeros. Converting these placeholder values would
			// make compression worse so don't touch them.
			if (src == 0)
				continue;

			const uint32_t pc = now_pos + (uint32_t)(i);

			uint32_t dest;
			if (is_encoder)
				dest = pc + src;
			else
				dest = src - pc;

			dest &= ADDR28_MASK;

			// Sign-extend negative values or unset sign bits
			// from positive values.
			if (dest & sign_bit)
				dest |= sign_mask;
			else
				dest &= ~sign_mask;

			assert((dest & sign_mask) == 0
					|| (dest & sign_mask) == sign_mask);

			// Since also the decoder will ignore src values
			// of 0, we must ensure that nothing is ever encoded
			// to 0. This is achieved by encoding such values
			// as pc instead. When decoding, pc will be first
			// converted to 0 which we will catch here and fix.
			if (dest == 0) {
				// We cannot get here if pc is zero because
				// then src would need to be zero too but we
				// already ensured that src != 0.
				assert((pc & ADDR28_MASK) != 0);
				dest = is_encoder ? pc : 0U - pc;
				dest &= ADDR28_MASK;

				if (dest & sign_bit)
					dest |= sign_mask;
				else
					dest &= ~sign_mask;
			}

			assert((dest & sign_mask) == 0
					|| (dest & sign_mask) == sign_mask);
			assert((dest & ~ADDR28_MASK) == 0);

			// Construct and store the modified 32-bit instruction.
			dest >>= 2;
			dest |= (uint32_t)ARM64_BL_OPCODE << 26;
			write32le(buffer + i, dest);
		}
	}

	return i;
}


#ifdef HAVE_ENCODER_ARM64
extern lzma_ret
lzma_arm64_props_encode(const void *options, uint8_t *out)
{
	const lzma_options_arm64 *const opt = options;

	if (opt->width < LZMA_ARM64_WIDTH_MIN
			|| opt->width > LZMA_ARM64_WIDTH_MAX)
		return LZMA_OPTIONS_ERROR;

	out[0] = (uint8_t)(opt->width - LZMA_ARM64_WIDTH_MIN);
	return LZMA_OK;
}
#endif


#ifdef HAVE_DECODER_ARM64
extern lzma_ret
lzma_arm64_props_decode(void **options, const lzma_allocator *allocator,
		const uint8_t *props, size_t props_size)
{
	if (props_size != 1)
		return LZMA_OPTIONS_ERROR;

	if (props[0] > LZMA_ARM64_WIDTH_MAX - LZMA_ARM64_WIDTH_MIN)
		return LZMA_OPTIONS_ERROR;

	lzma_options_arm64 *opt = lzma_alloc(sizeof(lzma_options_arm64),
			allocator);
	if (opt == NULL)
		return LZMA_MEM_ERROR;

	opt->width = props[0] + LZMA_ARM64_WIDTH_MIN;
	*options = opt;
	return LZMA_OK;

}
#endif


static lzma_ret
arm64_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	if (filters[0].options == NULL)
		return LZMA_PROG_ERROR;

	const lzma_options_arm64 *opt = filters[0].options;
	if (opt->width < LZMA_ARM64_WIDTH_MIN
			|| opt->width > LZMA_ARM64_WIDTH_MAX)
		return LZMA_OPTIONS_ERROR;

	const lzma_ret ret = lzma_simple_coder_init(next, allocator, filters,
			&arm64_code, sizeof(lzma_simple_arm64), 4, 4,
			is_encoder, false);

	if (ret == LZMA_OK) {
		lzma_simple_coder *coder = next->coder;
		lzma_simple_arm64 *simple = coder->simple;

		// This will be used to detect if the value, after
		// conversion has been done, is negative. The location
		// of the sign bit depends on the conversion width.
		simple->sign_bit = UINT32_C(1) << (opt->width - 1);

		// When conversion width isn't the maximum, the highest
		// bits must all be either zero or one, that is, they
		// all are copies of the sign bit. This mask is used to
		// (1) detect if input value is in the range specified
		// by the conversion width and (2) clearing or setting
		// the high bits after conversion (integers can wrap around).
		simple->sign_mask = (UINT32_C(1) << 28) - simple->sign_bit;
	}

	return ret;
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
