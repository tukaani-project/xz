/**
 * \file        lzma/alignment.h
 * \brief       Calculating input and output alignment of filter chains
 *
 * \author      Copyright (C) 1999-2006 Igor Pavlov
 * \author      Copyright (C) 2007 Lasse Collin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef LZMA_H_INTERNAL
#	error Never include this file directly. Use <lzma.h> instead.
#endif


/**
 * \brief       Calculates the preferred alignment of the input data
 *
 * FIXME desc
 */
extern uint32_t lzma_alignment_input(
		const lzma_options_filter *filters, uint32_t guess);


/**
 * \brief       Calculates the alignment of the encoded output
 *
 * Knowing the alignment of the output data is useful e.g. in the Block
 * encoder which tries to align the Compressed Data field optimally.
 *
 * \param       filters   Pointer to lzma_options_filter array, whose last
 *                        member must have .id = LZMA_VLI_VALUE_UNKNOWN.
 * \param       guess     The value to return if the alignment of the output
 *                        is the same as the alignment of the input data.
 *                        If you want to always detect this special case,
 *                        this guess to zero; this function never returns
 *                        zero unless guess is zero.
 *
 * \return      In most cases, a small positive integer is returned;
 *              for optimal use, the encoded output of this filter
 *              chain should start at on offset that is a multiple of
 *              the returned integer value.
 *
 *              If the alignment of the output is the same as the input
 *              data (which this function cannot know), \a guess is
 *              returned.
 *
 *              If an error occurs (that is, unknown Filter IDs or filter
 *              options), UINT32_MAX is returned.
 */
extern uint32_t lzma_alignment_output(
		const lzma_options_filter *filters, uint32_t guess);
