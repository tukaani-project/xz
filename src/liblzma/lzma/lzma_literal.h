///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_literal.h
/// \brief      Literal Coder
///
/// This is used as is by both LZMA encoder and decoder.
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

#ifndef LZMA_LITERAL_H
#define LZMA_LITERAL_H

#include "common.h"

// We need typedef of `probability'.
#include "range_common.h"


/// Each literal coder is divided in three sections:
///   - 0x001-0x0FF: Without match byte
///   - 0x101-0x1FF: With match byte; match bit is 0
///   - 0x201-0x2FF: With match byte; match bit is 1
#define LIT_SIZE 0x300

/// Calculate how many states are needed. Each state has
/// LIT_SIZE `probability' variables.
#define literal_states(literal_context_bits, literal_pos_bits) \
	(1U << ((literal_context_bits) + (literal_pos_bits)))

/// Locate the literal coder for the next literal byte. The choice depends on
///   - the lowest literal_pos_bits bits of the position of the current
///     byte; and
///   - the highest literal_context_bits bits of the previous byte.
#define literal_get_subcoder(literal_coder, pos, prev_byte) \
	(literal_coder)->coders[(((pos) & (literal_coder)->literal_pos_mask) \
			<< (literal_coder)->literal_context_bits) \
		+ ((prev_byte) >> (8 - (literal_coder)->literal_context_bits))]


typedef struct {
	uint32_t literal_context_bits;
	uint32_t literal_pos_bits;

	/// literal_pos_mask is always (1 << literal_pos_bits) - 1.
	uint32_t literal_pos_mask;

	/// There are (1 << (literal_pos_bits + literal_context_bits))
	/// literal coders.
	probability coders[][LIT_SIZE];

} lzma_literal_coder;


extern lzma_ret lzma_literal_init(
		lzma_literal_coder **coder, lzma_allocator *allocator,
		uint32_t literal_context_bits, uint32_t literal_pos_bits);

extern void lzma_literal_end(
		lzma_literal_coder **coder, lzma_allocator *allocator);

#endif
