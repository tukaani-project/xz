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

	now_pos += 5;

	lzma_simple_x86 *simple = simple_ptr;
	uint32_t prev_mask = simple->prev_mask;
	size_t prev_pos = 0;

	size_t i;
	for (i = 0; i < size; ++i) {
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

		prev_pos = i - prev_pos;
		prev_mask = prev_pos > 3 ? 0 : prev_mask >> prev_pos;
		prev_pos = i;

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
			prev_mask |= 1 << 3;
		}
	}

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
