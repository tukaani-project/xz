/**
 * \file        lzma/vli.h
 * \brief       Variable-length integer handling
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
 * \brief       Maximum supported value of variable-length integer
 */
#define LZMA_VLI_VALUE_MAX (UINT64_MAX / 2)

/**
 * \brief       VLI value to denote that the value is unknown
 */
#define LZMA_VLI_VALUE_UNKNOWN UINT64_MAX

/**
 * \brief       Maximum supported length of variable length integers
 */
#define LZMA_VLI_BYTES_MAX 9


/**
 * \brief       VLI constant suffix
 */
#define LZMA_VLI_C(n) UINT64_C(n)


/**
 * \brief       Variable-length integer type
 *
 * This will always be unsigned integer. Valid VLI values are in the range
 * [0, LZMA_VLI_VALUE_MAX]. Unknown value is indicated with
 * LZMA_VLI_VALUE_UNKNOWN, which is the maximum value of the underlaying
 * integer type (this feature is useful in several situations).
 *
 * In future, even if lzma_vli is typdefined to something else than uint64_t,
 * it is guaranteed that 2 * LZMA_VLI_VALUE_MAX will not overflow lzma_vli.
 * This simplifies integer overflow detection.
 */
typedef uint64_t lzma_vli;


/**
 * \brief       Simple macro to validate variable-length integer
 *
 * This is useful to test that application has given acceptable values
 * for example in the uncompressed_size and compressed_size variables.
 *
 * \return      True if the integer is representable as VLI or if it
 *              indicates unknown value.
 */
#define lzma_vli_is_valid(vli) \
	((vli) <= LZMA_VLI_VALUE_MAX || (vli) == LZMA_VLI_VALUE_UNKNOWN)


/**
 * \brief       Sets VLI to given value with error checking
 *
 * \param       dest    Target variable which must have type of lzma_vli.
 * \param       src     New value to be stored to dest.
 * \param       limit   Maximum allowed value for src.
 *
 * \return      False on success, true on error. If an error occurred,
 *              dest is left in undefined state (i.e. it's possible that
 *              it will be different in newer liblzma versions).
 */
#define lzma_vli_set_lim(dest, src, limit) \
	((src) > (limit) || ((dest) = (src)) > (limit))

/**
 * \brief
 */
#define lzma_vli_add_lim(dest, src, limit) \
	((src) > (limit) || ((dest) += (src)) > (limit))

#define lzma_vli_add2_lim(dest, src1, src2, limit) \
	(lzma_vli_add_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit))

#define lzma_vli_add3_lim(dest, src1, src2, src3, limit) \
	(lzma_vli_add_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit) \
		|| lzma_vli_add_lim(dest, src3, limit))

#define lzma_vli_add4_lim(dest, src1, src2, src3, src4, limit) \
	(lzma_vli_add_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit) \
		|| lzma_vli_add_lim(dest, src3, limit) \
		|| lzma_vli_add_lim(dest, src4, limit))

#define lzma_vli_sum_lim(dest, src1, src2, limit) \
	(lzma_vli_set_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit))

#define lzma_vli_sum3_lim(dest, src1, src2, src3, limit) \
	(lzma_vli_set_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit) \
		|| lzma_vli_add_lim(dest, src3, limit))

#define lzma_vli_sum4_lim(dest, src1, src2, src3, src4, limit) \
	(lzma_vli_set_lim(dest, src1, limit) \
		|| lzma_vli_add_lim(dest, src2, limit) \
		|| lzma_vli_add_lim(dest, src3, limit) \
		|| lzma_vli_add_lim(dest, src4, limit))

#define lzma_vli_set(dest, src) lzma_vli_set_lim(dest, src, LZMA_VLI_VALUE_MAX)

#define lzma_vli_add(dest, src) lzma_vli_add_lim(dest, src, LZMA_VLI_VALUE_MAX)

#define lzma_vli_add2(dest, src1, src2) \
	lzma_vli_add2_lim(dest, src1, src2, LZMA_VLI_VALUE_MAX)

#define lzma_vli_add3(dest, src1, src2, src3) \
	lzma_vli_add3_lim(dest, src1, src2, src3, LZMA_VLI_VALUE_MAX)

#define lzma_vli_add4(dest, src1, src2, src3, src4) \
	lzma_vli_add4_lim(dest, src1, src2, src3, src4, LZMA_VLI_VALUE_MAX)

#define lzma_vli_sum(dest, src1, src2) \
	lzma_vli_sum_lim(dest, src1, src2, LZMA_VLI_VALUE_MAX)

#define lzma_vli_sum3(dest, src1, src2, src3) \
	lzma_vli_sum3_lim(dest, src1, src2, src3, LZMA_VLI_VALUE_MAX)

#define lzma_vli_sum4(dest, src1, src2, src3, src4) \
	lzma_vli_sum4_lim(dest, src1, src2, src3, src4, LZMA_VLI_VALUE_MAX)


/**
 * \brief       Encodes variable-length integer
 *
 * In the new .lzma format, most integers are encoded in variable-length
 * representation. This saves space when smaller values are more likely
 * than bigger values.
 *
 * The encoding scheme encodes seven bits to every byte, using minimum
 * number of bytes required to represent the given value. In other words,
 * it puts 7-63 bits into 1-9 bytes. This implementation limits the number
 * of bits used to 63, thus num must be at maximum of UINT64_MAX / 2. You
 * may use LZMA_VLI_VALUE_MAX for clarity.
 *
 * \param       vli       Integer to be encoded
 * \param       vli_pos   How many VLI-encoded bytes have already been written
 *                        out. When starting to encode a new integer, *vli_pos
 *                        must be set to zero. To use single-call encoding,
 *                        set vli_pos to NULL.
 * \param       out       Beginning of the output buffer
 * \param       out_pos   The next byte will be written to out[*out_pos].
 * \param       out_size  Size of the out buffer; the first byte into
 *                        which no data is written to is out[out_size].
 *
 * \return      Slightly different return values are used in multi-call and
 *              single-call modes.
 *
 *              Multi-call (vli_pos != NULL):
 *              - LZMA_OK: So far all OK, but the integer is not
 *                completely written out yet.
 *              - LZMA_STREAM_END: Integer successfully encoded.
 *              - LZMA_PROG_ERROR: Arguments are not sane. This can be due
 *                to no *out_pos == out_size; this function doesn't use
 *                LZMA_BUF_ERROR.
 *
 *              Single-call (vli_pos == NULL):
 *              - LZMA_OK: Integer successfully encoded.
 *              - LZMA_PROG_ERROR: Arguments are not sane. This can be due
 *                to too little output space; this function doesn't use
 *                LZMA_BUF_ERROR.
 */
extern lzma_ret lzma_vli_encode(
		lzma_vli vli, size_t *lzma_restrict vli_pos,
		uint8_t *lzma_restrict out, size_t *lzma_restrict out_pos,
		size_t out_size);


/**
 * \brief       Decodes variable-length integer
 *
 * \param       vli       Pointer to decoded integer. The decoder will
 *                        initialize it to zero when *vli_pos == 0, so
 *                        application isn't required to initialize *vli.
 * \param       vli_pos   How many bytes have already been decoded. When
 *                        starting to decode a new integer, *vli_pos must
 *                        be initialized to zero. To use single-call decoding,
 *                        set this to NULL.
 * \param       in        Beginning of the input buffer
 * \param       in_pos    The next byte will be read from in[*in_pos].
 * \param       in_size   Size of the input buffer; the first byte that
 *                        won't be read is in[in_size].
 *
 * \return      Slightly different return values are used in multi-call and
 *              single-call modes.
 *
 *              Multi-call (vli_pos != NULL):
 *              - LZMA_OK: So far all OK, but the integer is not
 *                completely decoded yet.
 *              - LZMA_STREAM_END: Integer successfully decoded.
 *              - LZMA_DATA_ERROR: Integer is corrupt.
 *              - LZMA_PROG_ERROR: Arguments are not sane. This can be
 *                due to *in_pos == in_size; this function doesn't use
 *                LZMA_BUF_ERROR.
 *
 *              Single-call (vli_pos == NULL):
 *              - LZMA_OK: Integer successfully decoded.
 *              - LZMA_DATA_ERROR: Integer is corrupt.
 *              - LZMA_PROG_ERROR: Arguments are not sane. This can be due to
 *                too little input; this function doesn't use LZMA_BUF_ERROR.
 */
extern lzma_ret lzma_vli_decode(lzma_vli *lzma_restrict vli,
		size_t *lzma_restrict vli_pos, const uint8_t *lzma_restrict in,
		size_t *lzma_restrict in_pos, size_t in_size);


/**
 * \brief       Gets the number of bytes required to encode vli
 *
 * \return      Number of bytes on success (1-9). If vli isn't valid,
 *              zero is returned.
 */
extern uint32_t lzma_vli_size(lzma_vli vli);
