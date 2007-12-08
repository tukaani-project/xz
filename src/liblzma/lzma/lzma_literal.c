///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_literal.c
/// \brief      Literal Coder
//
//  Copyright (C) 1999-2006 Igor Pavlov
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "lzma_literal.h"


extern lzma_ret
lzma_literal_init(lzma_literal_coder **coder, lzma_allocator *allocator,
		uint32_t literal_context_bits, uint32_t literal_pos_bits)
{
	// Verify that arguments are sane.
	if (literal_context_bits > LZMA_LITERAL_CONTEXT_BITS_MAX
			|| literal_pos_bits > LZMA_LITERAL_POS_BITS_MAX)
		return LZMA_HEADER_ERROR;

	// Calculate the number of states the literal coder must store.
	const uint32_t states = literal_states(
			literal_pos_bits, literal_context_bits);

	// Allocate a new literal coder, if needed.
	if (*coder == NULL || (**coder).literal_context_bits
				!= literal_context_bits
			|| (**coder).literal_pos_bits != literal_pos_bits) {
		// Free the old coder, if any.
		lzma_free(*coder, allocator);

		// Allocate a new one.
		*coder = lzma_alloc(sizeof(lzma_literal_coder)
				+ states * LIT_SIZE * sizeof(probability),
				allocator);
		if (*coder == NULL)
			return LZMA_MEM_ERROR;

		// Store the new settings.
		(**coder).literal_context_bits = literal_context_bits;
		(**coder).literal_pos_bits = literal_pos_bits;

		// Calculate also the literal_pos_mask. It's not changed
		// anywhere else than here.
		(**coder).literal_pos_mask = (1 << literal_pos_bits) - 1;
	}

	// Reset the literal coder.
	for (uint32_t i = 0; i < states; ++i)
		for (uint32_t j = 0; j < LIT_SIZE; ++j)
			bit_reset((**coder).coders[i][j]);

	return LZMA_OK;
}


extern void
lzma_literal_end(lzma_literal_coder **coder, lzma_allocator *allocator)
{
	lzma_free(*coder, allocator);
	*coder = NULL;
}
