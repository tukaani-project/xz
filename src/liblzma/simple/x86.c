// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       x86.c
/// \brief      Filter for x86 binaries (BCJ filter)
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"

/*

x86 filter
==========

    This filter converts "CALL rel32" and "JMP rel32" instructions whose
    32-bit displacement is in the range +/-16 MiB, that is, the most
    significant byte equals 0x00 or 0xFF:

        CALL:  E8 xx xx xx 00
               E8 xx xx xx FF
        JMP:   E9 xx xx xx 00
               E9 xx xx xx FF

    If a conversion is skipped because the most significant byte isn't
    0x00 or 0xFF, we must ensure that the byte won't become 0x00 or 0xFF
    if a later CALL/JMP candidate contains that byte. For example, when
    converting the second CALL (E8) below, the byte AA must not become
    00 or FF so that it won't become a pair with the first E8:

            E8 55 E8 55 AA 55 00
        A:  ^-----------^
        B:        ^-----------^

        A = Skip because the most significant byte isn't 00 or FF.
        B = Convert while ensuring that AA doesn't become 00 or FF.

    Such protected bytes are tracked with the lowest bits in prev_mask.
    This is equivalent to tracking which of the previous three bytes
    were 0xE8/0xE9 (excluding bytes belonging to converted addresses).
    For example, if prev_mask is 4, then the previous byte was
    0xE8/0xE9 and the two bytes before that were not. If prev_mask
    is 7, then the three previous bytes were all 0xE8/0xE9.

    The code, that ensures that a protected byte doesn't become
    0x00 or 0xFF, has limitations:

        (1) It only works with one protected byte.

        (2) It only works if the original input byte isn't 0x00 or 0xFF.

    Due to (1), conversion must be skipped if there is more than one
    protected byte (prev_mask has more than one bit set). These skips
    can protect bytes that are 0x00 or 0xFF, so due to (2), conversion
    must be skipped if the protected byte is 0x00 or 0xFF:

            E8 E8 55 E8 55 E8 55 FF 55 00    prev_mask
        X:  ^-----------^                        0
        X:     ^-----------^                     4
        Y:           ^-----------^               3
        Z:                 ^-----------^         2

        X = Skip because the most significant byte isn't 00 or FF.
        Y = Skip because prev_mask has more than one bit set. We will
            also protect the byte FF even though the decision to skip
            didn't depend on it. We will keep skipping conversions
            until the previous three bytes contain at most one E8/E9.
        Z = Skip because the protected byte is FF. Also, the byte 00
            will become protected.

    So in some cases we protect bytes that wouldn't need to be
    protected. This makes no practical difference in compression ratio.

Conversion with a protected byte
--------------------------------

    In the normal conversion, the encoder adds the current program
    counter (pc) to the relative address in the CALL/JMP displacement,
    and the decoder subtracts pc from the displacement:

        if (is_encoder)
            dest = src + pc;
        else
            dest = src - pc;

    If there is a protected byte and the normal conversion produces
    a 0x00 or 0xFF in the protected byte, an extra step is required
    to change it to some other value. The extra step must produce such
    a value that a reverse normal conversion will produce 0x00 or 0xFF.
    That 0x00/0xFF will indicate that a reverse extra step is required.

    It is assumed that the protected byte isn't 0x00 or 0xFF in src.

    (1) The simplest method:

            dest = ~src;

        Note:

            ~src == (0 - src - 1)

            ~src == ~(src + pc) + pc == ~(src - pc) - pc

        Because the value doesn't depend on pc, the filtering benefit
        is lost.

    (2) Only modify the the lowest 1-3 bytes:

            m = prev_mask == 1 ? 0xFF
              : prev_mask == 2 ? 0xFFFF
              : 0xFFFFFF; // prev_mask == 4

            dest = src ^ m;

    (3) Filter the most significant bytes normally:

            dest = (dest & ~m) | (~src & m);

        Changing the low bits this way preserves carry/borrow behavior:
        if and only if ((src & m) + (pc & m)) produces a carry bit,
        then ((~src & m) - (pc & m)) borrows.

        This might be the best method.

    (4) The method used in this filter:

            if (is_encoder)
                dest = ((src + pc) ^ m) + pc;
            else
                dest = ((src - pc) ^ m) - pc;

        It makes (dest & m) == (~src & m), but effect on the the upper
        bytes is strange:

          - If pc is very small (upper bytes are zero), this method,
            (3), and (2) might set the upper bytes to the same values.
            In this method and in (3), it depends on carry/borrow luck.
            But pc doesn't stay very small unless the input is tiny.

          - When pc grows bigger, the upper bytes become scrambled
            because pc is added twice. The upper bytes aren't properly
            filtered and they don't have the original value from src
            either. So the effect on compression is closer to (1).

    The good news is that most conversions are done when there is no
    protected byte, and most conversions with a protected byte don't
    need the extra step. Any method from (1) to (4) is good enough;
    the difference in compression ratio with real-world files is noise.

*/


/// Test if the lowest 8 bits of b equal 0 or 255.
#define x86_test_msbyte(b) ((((b) + 1) & 0xFE) == 0)


typedef struct {
	uint32_t prev_mask;
} lzma_simple_x86;


static size_t
x86_code(void *simple_ptr, uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	if (size <= 4)
		return 0;

	size -= 4;

	// In the five-byte-long "CALL rel32" and "JMP rel32" instructions
	// the displacement is relative to the next instruction.
	now_pos += 5;

	lzma_simple_x86 *simple = simple_ptr;
	uint32_t prev_mask = simple->prev_mask;

	// prev_mask is updated only when needed. The current prev_mask value
	// is correct for buffer[prev_pos].
	size_t prev_pos = 0;

	size_t i;
	for (i = 0; i < size; ++i) {
		// Look for CALL (0xE8) or JMP (0xE9).
		//
		// We have at least five bytes of input available, so we can
		// compare multiple bytes before checking for i < size again.
		// Unaligned loads with read32le() would help a little with
		// specific inputs on some systems, but usually the
		// difference would be tiny, especially when when taking
		// the LZMA2 decoding time into account too.
#if defined(HAVE_SMALL)
		if ((buffer[i] & 0xFE) != 0xE8)
			continue;
#else
		if ((buffer[i] & 0xFE) != 0xE8) {
			if ((buffer[++i] & 0xFE) != 0xE8
					&& (buffer[++i] & 0xFE) != 0xE8
					&& (buffer[++i] & 0xFE) != 0xE8
					&& (buffer[++i] & 0xFE) != 0xE8)
				continue;

			if (i >= size)
				break;
		}
#endif

		// Calculate the distance from the previous CALL/JMP candidate
		// and shift the bits in prev_mask by that amount. Only the
		// the lowest four bits might have been set, so if the
		// distance is greater than 3, then the result is zero.
		prev_pos = i - prev_pos;
		prev_mask = prev_pos > 3 ? 0 : prev_mask >> prev_pos;
		prev_pos = i;

		// Convert only if the CALL/JMP displacement is +/-16 MiB,
		// that is, the most significant byte is 0x00 or 0xFF, and
		//   - we haven't skipped any CALL/JMP candidates in the
		//     previous three bytes (prev_mask == 0), or
		//   - we have skipped exactly one candidate in the previous
		//     three bytes (prev_mask has exactly one bit set), and
		//     the byte protected by prev_mask isn't 0x00 or 0xFF.
		if (x86_test_msbyte(buffer[i + 4]) && (prev_mask == 0
				|| (prev_mask <= 4 && prev_mask != 3
					&& !x86_test_msbyte(buffer[
						i + 1 + (prev_mask >> 1)])))) {
			const uint32_t src = read32le(buffer + i + 1);

			uint32_t pc = now_pos + (uint32_t)i;
			if (!is_encoder)
				pc = 0U - pc;

			uint32_t dest = src + pc;

			if (prev_mask != 0) {
				// Ensure that the protected byte doesn't
				// become 0x00 or 0xFF. This only works if
				// the original byte in src isn't 0x00/0xFF.
				const uint32_t j = (prev_mask & 6) << 2;
				if (x86_test_msbyte(dest >> j)) {
					dest ^= (UINT32_C(0x100) << j) - 1;
					dest += pc;
					assert(!x86_test_msbyte(dest >> j));
				}
			}

			dest &= 0x01FFFFFF;
			dest |= 0U - (dest & 0x01000000);
			write32le(buffer + i + 1, dest);
			i += 4;
		} else {
			// Mark that there was 0xE8 or 0xE9 in buffer[i]
			// and that the byte at buffer[i + 4] is protected.
			prev_mask |= 1 << 3;
		}
	}

	// Update prev_mask to match the number of filtered bytes.
	// This way we don't need to preserve a variant of prev_pos
	// across calls to this function.
	prev_pos = i - prev_pos;
	simple->prev_mask = prev_pos > 3 ? 0 : prev_mask >> prev_pos;
	return i;
}


static lzma_ret
x86_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	const lzma_ret ret = lzma_simple_coder_init(next, allocator, filters,
			&x86_code, sizeof(lzma_simple_x86), 5, 1, is_encoder);

	if (ret == LZMA_OK) {
		lzma_simple_coder *coder = next->coder;
		lzma_simple_x86 *simple = coder->simple;
		simple->prev_mask = 0;
	}

	return ret;
}


#ifdef HAVE_ENCODER_X86
extern lzma_ret
lzma_simple_x86_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return x86_coder_init(next, allocator, filters, true);
}


extern LZMA_API(size_t)
lzma_bcj_x86_encode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	lzma_simple_x86 simple = {
		.prev_mask = 0,
	};

	return x86_code(&simple, start_offset, true, buf, size);
}
#endif


#ifdef HAVE_DECODER_X86
extern lzma_ret
lzma_simple_x86_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return x86_coder_init(next, allocator, filters, false);
}


extern LZMA_API(size_t)
lzma_bcj_x86_decode(uint32_t start_offset, uint8_t *buf, size_t size)
{
	lzma_simple_x86 simple = {
		.prev_mask = 0,
	};

	return x86_code(&simple, start_offset, false, buf, size);
}
#endif
