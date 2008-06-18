/**
 * \file        lzma/filter.h
 * \brief       Common filter related types
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
 * \brief       Filter options
 *
 * This structure is used to pass Filter ID and a pointer filter's options
 * to liblzma.
 */
typedef struct {
	/**
	 * \brief       Filter ID
	 *
	 * Use constants whose name begin with `LZMA_FILTER_' to specify
	 * different filters. In an array of lzma_option_filter structures,
	 * use LZMA_VLI_VALUE_UNKNOWN to indicate end of filters.
	 */
	lzma_vli id;

	/**
	 * \brief       Pointer to filter-specific options structure
	 *
	 * If the filter doesn't need options, set this to NULL. If id is
	 * set to LZMA_VLI_VALUE_UNKNOWN, options is ignored, and thus
	 * doesn't need be initialized.
	 *
	 * Some filters support changing the options in the middle of
	 * the encoding process. These filters store the pointer of the
	 * options structure and communicate with the application via
	 * modifications of the options structure.
	 */
	void *options;

} lzma_options_filter;


/**
 * \brief       Filters available for encoding
 *
 * Pointer to an array containing the list of available Filter IDs that
 * can be used for encoding. The last element is LZMA_VLI_VALUE_UNKNOWN.
 *
 * If lzma_available_filter_encoders[0] == LZMA_VLI_VALUE_UNKNOWN, the
 * encoder components haven't been built at all. This means that the
 * encoding-specific functions are probably missing from the library
 * API/ABI completely.
 */
extern const lzma_vli *const lzma_available_filter_encoders;


/**
 * \brief       Filters available for decoding
 *
 * Pointer to an array containing the list of available Filter IDs that
 * can be used for decoding. The last element is LZMA_VLI_VALUE_UNKNOWN.
 *
 * If lzma_available_filter_decoders[0] == LZMA_VLI_VALUE_UNKNOWN, the
 * decoder components haven't been built at all. This means that the
 * decoding-specific functions are probably missing from the library
 * API/ABI completely.
 */
extern const lzma_vli *const lzma_available_filter_decoders;


/**
 * \brief       Calculate rough memory requirements for given filter chain
 *
 * \param       filters     Array of filters terminated with
 *                          .id == LZMA_VLI_VALUE_UNKNOWN.
 * \param       is_encoder  Set to true when calculating memory requirements
 *                          of an encoder; false for decoder.
 *
 * \return      Number of mebibytes (MiB i.e. 2^20) required for the given
 *              encoder or decoder filter chain.
 *
 * \note        If calculating memory requirements of encoder, lzma_init() or
 *              lzma_init_encoder() must have been called earlier. Similarly,
 *              if calculating memory requirements of decoder, lzma_init() or
 *              lzma_init_decoder() must have been called earlier.
 */
extern uint32_t lzma_memory_usage(
		const lzma_options_filter *filters, lzma_bool is_encoder);


/**
 * \brief       Calculates encoded size of a Filter Flags field
 *
 * Knowing the size of Filter Flags is useful to know when allocating
 * memory to hold the encoded Filter Flags.
 *
 * \param       size    Pointer to integer to hold the calculated size
 * \param       options Filter ID and associated options whose encoded
 *                      size is to be calculted
 *
 * \return      - LZMA_OK: *size set successfully. Note that this doesn't
 *                guarantee that options->options is valid, thus
 *                lzma_filter_flags_encode() may still fail.
 *              - LZMA_HEADER_ERROR: Unknown Filter ID or unsupported options.
 *              - LZMA_PROG_ERROR: Invalid options
 *
 * \note        If you need to calculate size of List of Filter Flags,
 *              you need to loop over every lzma_options_filter entry.
 */
extern lzma_ret lzma_filter_flags_size(
		uint32_t *size, const lzma_options_filter *options);


/**
 * \brief       Encodes Filter Flags into given buffer
 *
 * In contrast to some functions, this doesn't allocate the needed buffer.
 * This is due to how this function is used internally by liblzma.
 *
 * \param       out         Beginning of the output buffer
 * \param       out_pos     out[*out_pos] is the next write position. This
 *                          is updated by the encoder.
 * \param       out_size    out[out_size] is the first byte to not write.
 * \param       options     Filter options to be encoded
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_HEADER_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR: Invalid options or not enough output
 *                buffer space (you should have checked it with
 *                lzma_filter_flags_size()).
 */
extern lzma_ret lzma_filter_flags_encode(uint8_t *out, size_t *out_pos,
		size_t out_size, const lzma_options_filter *options);


/**
 * \brief       Initializes Filter Flags decoder
 *
 * The decoded result is stored into *options. options->options is
 * initialized but the old value is NOT free()d.
 *
 * Because the results of this decoder are placed into *options,
 * strm->next_in, strm->avail_in, and strm->total_in are not used
 * when calling lzma_code(). The only valid action for lzma_code()
 * is LZMA_RUN
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_filter_flags_decode(
		lzma_options_filter *options, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size);
