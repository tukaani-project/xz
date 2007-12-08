/**
 * \file        lzma/extra.h
 * \brief       Handling of Extra Records in Metadata
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


/*
 * Extra Record IDs
 *
 * See the .lzma file format specification for description what each
 * Extra Record type exactly means.
 *
 * If you ever need to update .lzma files with Extra Records, note that
 * the Record IDs are divided in two categories:
 *  - Safe-to-Copy Records may be preserved as is when the
 *    Stream is modified in ways that don't change the actual
 *    uncompressed data. Examples of such operatings include
 *    recompressing and adding, modifying, or deleting unrelated
 *    Extra Records.
 *  - Unsafe-to-Copy Records should be removed (and possibly
 *    recreated) when any kind of changes are made to the Stream.
 */

#define LZMA_EXTRA_PADDING      0x00
#define LZMA_EXTRA_OPENPGP      0x01
#define LZMA_EXTRA_FILTERS      0x02
#define LZMA_EXTRA_COMMENT      0x03
#define LZMA_EXTRA_CHECKS       0x04
#define LZMA_EXTRA_FILENAME     0x05
#define LZMA_EXTRA_MTIME        0x07
#define LZMA_EXTRA_MTIME_HR     0x09
#define LZMA_EXTRA_MIME_TYPE    0x0B
#define LZMA_EXTRA_HOMEPAGE     0x0D


/**
 * \brief       Extra Records
 *
 * The .lzma format provides a way to store custom information along
 * the actual compressed content. Information about these Records
 * are passed to and from liblzma via this linked list.
 */
typedef struct lzma_extra_s lzma_extra;
struct lzma_extra_s {
	/**
	 * \brief       Pointer to the next Extra Record
	 *
	 * This is NULL on the last Extra Record.
	 */
	lzma_extra *next;

	/**
	 * \brief       Record ID
	 *
	 * Extra Record IDs are divided in three categories:
	 *   - Zero is a special case used for padding. It doesn't have
	 *     Size of Data fields.
	 *   - Odd IDs (1, 3, 5, ...) are Safe-to-Copy IDs.
	 *     These can be preserved as is if the Stream is
	 *     modified in a way that doesn't alter the actual
	 *     uncompressed content.
	 *   - Even IDs (2, 4, 6, ...) are Unsafe-to-Copy IDs.
	 *     If the .lzma Stream is modified in any way,
	 *     the Extra Records having a sensitive ID should
	 *     be removed or updated accordingly.
	 *
	 * Refer to the .lzma file format specification for
	 * the up to date list of Extra Record IDs.
	 */
	lzma_vli id;

	/**
	 * \brief       Size of the Record data
	 *
	 * In case of strings, this should not include the
	 * trailing '\0'.
	 */
	size_t size;

	/**
	 * \brief       Record data
	 *
	 * Record data is often a string in UTF-8 encoding,
	 * but it can be arbitrary binary data. In case of
	 * strings, the trailing '\0' is usually not stored
	 * in the .lzma file.
	 *
	 * To ease working with Extra Records containing strings,
	 * liblzma always adds '\0' to the end of data even when
	 * it wasn't present in the .lzma file. This '\0' is not
	 * counted in the size of the data.
	 */
	uint8_t *data;
};


extern void lzma_extra_free(lzma_extra *extra, lzma_allocator *allocator);
