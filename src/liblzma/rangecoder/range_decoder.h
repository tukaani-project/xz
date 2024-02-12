// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       range_decoder.h
/// \brief      Range Decoder
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_RANGE_DECODER_H
#define LZMA_RANGE_DECODER_H

#include "range_common.h"


// Negative RC_BIT_MODEL_TOTAL but the lowest RC_MOVE_BITS are flipped.
// This is useful for updating probability variables in branchless decoding:
//
//     uint32_t decoded_bit = ...;
//     probability tmp = RC_BIT_MODEL_OFFSET;
//     tmp &= decoded_bit - 1;
//     prob -= (prob + tmp) >> RC_MOVE_BITS;
#define RC_BIT_MODEL_OFFSET \
	((UINT32_C(1) << RC_MOVE_BITS) - 1 - RC_BIT_MODEL_TOTAL)


typedef struct {
	uint32_t range;
	uint32_t code;
	uint32_t init_bytes_left;
} lzma_range_decoder;


/// Reads the first five bytes to initialize the range decoder.
static inline lzma_ret
rc_read_init(lzma_range_decoder *rc, const uint8_t *restrict in,
		size_t *restrict in_pos, size_t in_size)
{
	while (rc->init_bytes_left > 0) {
		if (*in_pos == in_size)
			return LZMA_OK;

		// The first byte is always 0x00. It could have been omitted
		// in LZMA2 but it wasn't, so one byte is wasted in every
		// LZMA2 chunk.
		if (rc->init_bytes_left == 5 && in[*in_pos] != 0x00)
			return LZMA_DATA_ERROR;

		rc->code = (rc->code << 8) | in[*in_pos];
		++*in_pos;
		--rc->init_bytes_left;
	}

	return LZMA_STREAM_END;
}


/// Makes local copies of range decoder and *in_pos variables. Doing this
/// improves speed significantly. The range decoder macros expect also
/// variables 'in' and 'in_size' to be defined.
#define rc_to_local(range_decoder, in_pos) \
	lzma_range_decoder rc = range_decoder; \
	const uint8_t *rc_in_ptr = in + (in_pos); \
	const uint8_t *rc_in_end = in + in_size; \
	uint32_t rc_bound


/// Stores the local copes back to the range decoder structure.
#define rc_from_local(range_decoder, in_pos) \
do { \
	range_decoder = rc; \
	in_pos = (size_t)(rc_in_ptr - in); \
} while (0)


/// Resets the range decoder structure.
#define rc_reset(range_decoder) \
do { \
	(range_decoder).range = UINT32_MAX; \
	(range_decoder).code = 0; \
	(range_decoder).init_bytes_left = 5; \
} while (0)


/// When decoding has been properly finished, rc.code is always zero unless
/// the input stream is corrupt. So checking this can catch some corrupt
/// files especially if they don't have any other integrity check.
#define rc_is_finished(range_decoder) \
	((range_decoder).code == 0)


// Read the next input byte if needed.
#define rc_normalize() \
do { \
	if (rc.range < RC_TOP_VALUE) { \
		rc.range <<= RC_SHIFT_BITS; \
		rc.code = (rc.code << RC_SHIFT_BITS) | *rc_in_ptr++; \
	} \
} while (0)


/// If more input is needed but there is
/// no more input available, "goto out" is used to jump out of the main
/// decoder loop. The "_safe" macros are used in the Resumable decoder
/// mode in order to save the sequence to continue decoding from that
/// point later.
#define rc_normalize_safe(seq) \
do { \
	if (rc.range < RC_TOP_VALUE) { \
		if (rc_in_ptr == rc_in_end) { \
			coder->sequence = seq; \
			goto out; \
		} \
		rc.range <<= RC_SHIFT_BITS; \
		rc.code = (rc.code << RC_SHIFT_BITS) | *rc_in_ptr++; \
	} \
} while (0)


/// Start decoding a bit. This must be used together with rc_update_0()
/// and rc_update_1():
///
///     rc_if_0(prob) {
///         rc_update_0(prob);
///         // Do something
///     } else {
///         rc_update_1(prob);
///         // Do something else
///     }
///
#define rc_if_0(prob) \
	rc_normalize(); \
	rc_bound = (rc.range >> RC_BIT_MODEL_TOTAL_BITS) * (prob); \
	if (rc.code < rc_bound)


#define rc_if_0_safe(prob, seq) \
	rc_normalize_safe(seq); \
	rc_bound = (rc.range >> RC_BIT_MODEL_TOTAL_BITS) * (prob); \
	if (rc.code < rc_bound)


/// Update the range decoder state and the used probability variable to
/// match a decoded bit of 0.
///
/// The x86-64 assemly uses the commented method but it seems that,
/// at least on x86-64, the first version is slightly faster as C code.
#define rc_update_0(prob) \
do { \
	rc.range = rc_bound; \
	prob += (RC_BIT_MODEL_TOTAL - (prob)) >> RC_MOVE_BITS; \
	/* prob -= ((prob) + RC_BIT_MODEL_OFFSET) >> RC_MOVE_BITS; */ \
} while (0)


/// Update the range decoder state and the used probability variable to
/// match a decoded bit of 1.
#define rc_update_1(prob) \
do { \
	rc.range -= rc_bound; \
	rc.code -= rc_bound; \
	prob -= (prob) >> RC_MOVE_BITS; \
} while (0)


/// Decodes one bit and runs action0 or action1 depending on the decoded bit.
/// This macro is used as the last step in bittree reverse decoders since
/// those don't use "symbol" for anything else than indexing the probability
/// arrays.
#define rc_bit_last(prob, action0, action1) \
do { \
	rc_if_0(prob) { \
		rc_update_0(prob); \
		action0; \
	} else { \
		rc_update_1(prob); \
		action1; \
	} \
} while (0)


#define rc_bit_last_safe(prob, action0, action1, seq) \
do { \
	rc_if_0_safe(prob, seq) { \
		rc_update_0(prob); \
		action0; \
	} else { \
		rc_update_1(prob); \
		action1; \
	} \
} while (0)


/// Decodes one bit, updates "symbol", and runs action0 or action1 depending
/// on the decoded bit.
#define rc_bit(prob, action0, action1) \
	rc_bit_last(prob, \
		symbol <<= 1; action0, \
		symbol = (symbol << 1) + 1; action1);


#define rc_bit_safe(prob, action0, action1, seq) \
	rc_bit_last_safe(prob, \
		symbol <<= 1; action0, \
		symbol = (symbol << 1) + 1; action1, \
		seq);

// Unroll fixed-sized bittree decoding.
//
// A compile-time constant in final_add can be used to get rid of the high bit
// from symbol that is used for the array indexing (1U << bittree_bits).
// final_add may also be used to add offset to the result (LZMA length
// decoder does that).
//
// The reason to have final_add here is that in the asm code the addition
// can be done for free: in x86-64 there is SBB instruction with -1 as
// the immediate value, and final_add is combined with that value.
#define rc_bittree_bit(prob) \
	rc_bit(prob, , )

#define rc_bittree3(probs, final_add) \
do { \
	symbol = 1; \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	symbol += (uint32_t)(final_add); \
} while (0)

#define rc_bittree6(probs, final_add) \
do { \
	symbol = 1; \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	symbol += (uint32_t)(final_add); \
} while (0)

#define rc_bittree8(probs, final_add) \
do { \
	symbol = 1; \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	rc_bittree_bit(probs[symbol]); \
	symbol += (uint32_t)(final_add); \
} while (0)


// Fixed-sized reverse bittree
#define rc_bittree_rev4(probs) \
do { \
	symbol = 0; \
	rc_bit_last(probs[symbol + 1], , symbol += 1); \
	rc_bit_last(probs[symbol + 2], , symbol += 2); \
	rc_bit_last(probs[symbol + 4], , symbol += 4); \
	rc_bit_last(probs[symbol + 8], , symbol += 8); \
} while (0)


// Decode one bit from variable-sized reverse bittree.
// The loop is done in the code that uses this macro.
#define rc_bit_add_if_1(probs, dest, value_to_add_if_1) \
	rc_bit(probs[symbol], \
		, \
		dest += value_to_add_if_1);


// Matched literal
#define decode_with_match_bit \
		t_match_byte <<= 1; \
		t_match_bit = t_match_byte & t_offset; \
		t_subcoder_index = t_offset + t_match_bit + symbol; \
		rc_bit(probs[t_subcoder_index], \
				t_offset &= ~t_match_bit, \
				t_offset &= t_match_bit)

#define rc_matched_literal(probs_base_var, match_byte) \
do { \
	uint32_t t_match_byte = (match_byte); \
	uint32_t t_match_bit; \
	uint32_t t_subcoder_index; \
	uint32_t t_offset = 0x100; \
	symbol = 1; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
	decode_with_match_bit; \
} while (0)


/// Decode a bit without using a probability.
//
// NOTE: GCC 13 and Clang/LLVM 16 can, at least on x86-64, optimize the bound
// calculation to use an arithmetic right shift so there's no need to provide
// the alternative code which, according to C99/C11/C23 6.3.1.3-p3 isn't
// perfectly portable: rc_bound = (uint32_t)((int32_t)rc.code >> 31);
#define rc_direct(dest, count_var) \
do { \
	dest = (dest << 1) + 1; \
	rc_normalize(); \
	rc.range >>= 1; \
	rc.code -= rc.range; \
	rc_bound = UINT32_C(0) - (rc.code >> 31); \
	dest += rc_bound; \
	rc.code += rc.range & rc_bound; \
} while (--count_var > 0)



#define rc_direct_safe(dest, count_var, seq) \
do { \
	rc_normalize_safe(seq); \
	rc.range >>= 1; \
	rc.code -= rc.range; \
	rc_bound = UINT32_C(0) - (rc.code >> 31); \
	rc.code += rc.range & rc_bound; \
	dest = (dest << 1) + (rc_bound + 1); \
} while (--count_var > 0)

#endif
