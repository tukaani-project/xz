///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_info.c
/// \brief      Tests functions handling the lzma_info structure
//
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

#include "tests.h"


static lzma_info *info = NULL;
static lzma_info_iter iter;

static lzma_vli stream_start = 0;
static lzma_vli header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
static lzma_vli total_size = LZMA_VLI_VALUE_UNKNOWN;
static lzma_vli uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
static lzma_vli footer_metadata_size = LZMA_VLI_VALUE_UNKNOWN;

static lzma_index my_index[3] = {
	{ 22, 33, my_index + 1 },
	{ 44, 55, my_index + 2 },
	{ 66, 77, NULL },
};

static lzma_metadata my_metadata = {
	.header_metadata_size = 11,
	.total_size = 22 + 44 + 66,
	.uncompressed_size = 33 + 55 + 77,
	.index = my_index,
	.extra = NULL,
};


static void
reset(void)
{
	expect(lzma_info_init(info, NULL) == info);
	stream_start = 0;
	header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	total_size = LZMA_VLI_VALUE_UNKNOWN;
	uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	footer_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
}


static void
validate(void)
{
	expect(lzma_info_size_get(info, LZMA_INFO_STREAM_START)
			== stream_start);
	expect(lzma_info_size_get(info, LZMA_INFO_HEADER_METADATA)
			== header_metadata_size);
	expect(lzma_info_size_get(info, LZMA_INFO_TOTAL) == total_size);
	expect(lzma_info_size_get(info, LZMA_INFO_UNCOMPRESSED)
			== uncompressed_size);
	expect(lzma_info_size_get(info, LZMA_INFO_FOOTER_METADATA)
			== footer_metadata_size);
}


static void
test1(void)
{
	// Basics
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START,
			stream_start = 1234) == LZMA_OK);
	validate();
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA,
			header_metadata_size = 2345) == LZMA_OK);
	validate();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, total_size = 3456)
			== LZMA_OK);
	validate();
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			uncompressed_size = 4567) == LZMA_OK);
	validate();
	expect(lzma_info_size_set(info, LZMA_INFO_FOOTER_METADATA,
			footer_metadata_size = 5432) == LZMA_OK);
	validate();

	// Not everything allow zero size
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START,
			stream_start = 0) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA,
			header_metadata_size = 0) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			uncompressed_size = 0) == LZMA_OK);
	validate();

	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 0)
			== LZMA_PROG_ERROR);

	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_FOOTER_METADATA, 0)
			== LZMA_PROG_ERROR);

	// Invalid sizes
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START,
			LZMA_VLI_VALUE_MAX + 1) == LZMA_PROG_ERROR);
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA,
			LZMA_VLI_VALUE_MAX + 1) == LZMA_PROG_ERROR);
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL,
			LZMA_VLI_VALUE_MAX + 1) == LZMA_PROG_ERROR);
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			LZMA_VLI_VALUE_MAX + 1) == LZMA_PROG_ERROR);
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_FOOTER_METADATA,
			LZMA_VLI_VALUE_MAX + 1) == LZMA_PROG_ERROR);

	reset();
}


static bool
test2_helper(lzma_vli *num, lzma_info_size type)
{
	expect(lzma_info_size_set(info, type, *num = 1234) == LZMA_OK);
	validate();
	const bool ret = lzma_info_size_set(info, type, 4321) != LZMA_OK;
	reset();
	return ret;
}


static void
test2(void)
{
	// Excluding start offset of Stream, once a size has been set,
	// trying to set some other known value fails.
	expect(!test2_helper(&stream_start, LZMA_INFO_STREAM_START));
	expect(test2_helper(&header_metadata_size, LZMA_INFO_HEADER_METADATA));
	expect(test2_helper(&total_size, LZMA_INFO_TOTAL));
	expect(test2_helper(&uncompressed_size, LZMA_INFO_UNCOMPRESSED));
	expect(test2_helper(&footer_metadata_size, LZMA_INFO_FOOTER_METADATA));
}


static void
test3_init(void)
{
	reset();
	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
}


static void
test3(void)
{
	// Setting the same sizes multiple times for the same Index Record
	// is OK, but the values must always be the same.
	test3_init();
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_set(&iter, 1111, 2345) == LZMA_DATA_ERROR);

	// Cannot finish an empty Index.
	test3_init();
	expect(lzma_info_index_finish(info) == LZMA_DATA_ERROR);

	test3_init();
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_index_finish(info) == LZMA_DATA_ERROR);

	test3_init();
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_index_finish(info) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 1234) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 2345)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 1111)
			== LZMA_DATA_ERROR);

	test3_init();
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_iter_set(&iter, 4321, 5432) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_index_finish(info) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 1234 + 4321)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 2345 + 5432)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 1111)
			== LZMA_DATA_ERROR);

	test3_init();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 1234 + 4321)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 2345 + 5432)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 1111)
			== LZMA_DATA_ERROR);
	expect(lzma_info_iter_set(&iter, 1234, 2345) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_iter_set(&iter, 4321, 5432) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 2);
	expect(lzma_info_index_finish(info) == LZMA_OK);

	test3_init();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, 1000) == LZMA_OK);
	expect(lzma_info_iter_set(&iter, 1001, 2001) == LZMA_DATA_ERROR);

	test3_init();
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED, 2000)
			== LZMA_OK);
	expect(lzma_info_iter_set(&iter, 1001, 2001) == LZMA_DATA_ERROR);

	reset();
}


static void
test4(void)
{
	// 4a
	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_index_count_get(info) == 0);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 1);

	expect(lzma_info_iter_set(&iter, 22, 33) == LZMA_OK);
	expect(iter.total_size == 22);
	expect(iter.uncompressed_size == 33);
	expect(iter.stream_offset == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 1);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_offset == 33);

	// 4b
	reset();
	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_index_count_get(info) == 0);
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START, 5) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA, 11)
			== LZMA_OK);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 1);

	expect(lzma_info_iter_set(&iter, 22, 33) == LZMA_OK);
	expect(iter.total_size == 22);
	expect(iter.uncompressed_size == 33);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 1);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11 + 22);
	expect(iter.uncompressed_offset == 33);
	expect(lzma_info_index_count_get(info) == 2);

	expect(lzma_info_iter_set(&iter, 44, 55) == LZMA_OK);
	expect(iter.total_size == 44);
	expect(iter.uncompressed_size == 55);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11 + 22);
	expect(iter.uncompressed_offset == 33);
	expect(lzma_info_index_count_get(info) == 2);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44);
	expect(iter.uncompressed_offset == 33 + 55);
	expect(lzma_info_index_count_get(info) == 3);

	expect(lzma_info_iter_set(&iter, 66, 77) == LZMA_OK);
	expect(iter.total_size == 66);
	expect(iter.uncompressed_size == 77);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44);
	expect(iter.uncompressed_offset == 33 + 55);
	expect(lzma_info_index_count_get(info) == 3);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66);
	expect(iter.uncompressed_offset == 33 + 55 + 77);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_set(&iter, 88, 99) == LZMA_OK);
	expect(iter.total_size == 88);
	expect(iter.uncompressed_size == 99);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66);
	expect(iter.uncompressed_offset == 33 + 55 + 77);
	expect(lzma_info_index_count_get(info) == 4);

	// 4c (continues from 4b)
	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == 22);
	expect(iter.uncompressed_size == 33);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_set(&iter, 22, LZMA_VLI_VALUE_UNKNOWN)
			== LZMA_OK);
	expect(iter.total_size == 22);
	expect(iter.uncompressed_size == 33);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == 44);
	expect(iter.uncompressed_size == 55);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11 + 22);
	expect(iter.uncompressed_offset == 33);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_set(&iter, LZMA_VLI_VALUE_UNKNOWN, 55)
			== LZMA_OK);
	expect(iter.total_size == 44);
	expect(iter.uncompressed_size == 55);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11 + 22);
	expect(iter.uncompressed_offset == 33);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == 66);
	expect(iter.uncompressed_size == 77);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44);
	expect(iter.uncompressed_offset == 33 + 55);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_set(&iter, LZMA_VLI_VALUE_UNKNOWN,
			LZMA_VLI_VALUE_UNKNOWN) == LZMA_OK);
	expect(iter.total_size == 66);
	expect(iter.uncompressed_size == 77);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44);
	expect(iter.uncompressed_offset == 33 + 55);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == 88);
	expect(iter.uncompressed_size == 99);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66);
	expect(iter.uncompressed_offset == 33 + 55 + 77);
	expect(lzma_info_index_count_get(info) == 4);

	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66 + 88);
	expect(iter.uncompressed_offset == 33 + 55 + 77 + 99);
	expect(lzma_info_index_count_get(info) == 5);

	expect(lzma_info_iter_set(&iter, 1234, LZMA_VLI_VALUE_UNKNOWN)
			== LZMA_OK);
	expect(iter.total_size == 1234);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66 + 88);
	expect(iter.uncompressed_offset == 33 + 55 + 77 + 99);
	expect(lzma_info_index_count_get(info) == 5);

	// Test 4d (continues from 4c)
	lzma_info_iter_begin(info, &iter);
	for (size_t i = 0; i < 4; ++i)
		expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(lzma_info_iter_set(&iter, 88, 99) == LZMA_OK);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.total_size == 1234);
	expect(iter.uncompressed_size == LZMA_VLI_VALUE_UNKNOWN);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66 + 88);
	expect(iter.uncompressed_offset == 33 + 55 + 77 + 99);
	expect(lzma_info_index_count_get(info) == 5);

	expect(lzma_info_iter_set(&iter, LZMA_VLI_VALUE_UNKNOWN, 4321)
			== LZMA_OK);
	expect(iter.total_size == 1234);
	expect(iter.uncompressed_size == 4321);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE
			+ 11 + 22 + 44 + 66 + 88);
	expect(iter.uncompressed_offset == 33 + 55 + 77 + 99);
	expect(lzma_info_index_count_get(info) == 5);

	expect(lzma_info_index_finish(info) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 5);

	// Test 4e (continues from 4d)
	lzma_info_iter_begin(info, &iter);
	for (size_t i = 0; i < 5; ++i)
		expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(lzma_info_iter_set(&iter, 1234, 4321) == LZMA_OK);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_DATA_ERROR);

	reset();
}


static void
test5(void)
{
	lzma_index *i;

	expect(lzma_info_index_set(info, NULL, NULL, true)
			== LZMA_PROG_ERROR);

	reset();
	expect(lzma_info_index_set(info, NULL, my_index, false) == LZMA_OK);
	i = lzma_index_dup(my_index, NULL);
	expect(i != NULL);
	i->next->uncompressed_size = 99;
	expect(lzma_info_index_set(info, NULL, i, true) == LZMA_DATA_ERROR);

	reset();
	expect(lzma_info_index_set(info, NULL, my_index, false) == LZMA_OK);
	i = lzma_index_dup(my_index, NULL);
	expect(i != NULL);
	lzma_index_free(i->next->next, NULL);
	i->next->next = NULL;
	expect(lzma_info_index_set(info, NULL, i, true) == LZMA_DATA_ERROR);

	reset();
	expect(lzma_info_index_set(info, NULL, my_index, false) == LZMA_OK);
	i = lzma_index_dup(my_index, NULL);
	expect(i != NULL);
	lzma_index_free(i->next->next, NULL);
	i->next->next = lzma_index_dup(my_index, NULL);
	expect(i->next->next != NULL);
	expect(lzma_info_index_set(info, NULL, i, true) == LZMA_DATA_ERROR);

	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL,
			total_size = 22 + 44 + 66) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			uncompressed_size = 33 + 55 + 77) == LZMA_OK);
	validate();
	expect(lzma_info_index_set(info, NULL, my_index, false) == LZMA_OK);
	validate();

	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL, total_size = 77)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			uncompressed_size = 33 + 55 + 77) == LZMA_OK);
	validate();
	expect(lzma_info_index_set(info, NULL, my_index, false)
			== LZMA_DATA_ERROR);

	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_TOTAL,
			total_size = 22 + 44 + 66) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_UNCOMPRESSED,
			uncompressed_size = 777777) == LZMA_OK);
	validate();
	expect(lzma_info_index_set(info, NULL, my_index, false)
			== LZMA_DATA_ERROR);

	reset();
}


static void
test6(void)
{
	lzma_metadata metadata;

	// Same complete Metadata in both Header and Footer
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA,
			my_metadata.header_metadata_size) == LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, false, false)
			== LZMA_OK);

	// Header Metadata is not present but Size of Header Metadata is
	// still present in Footer.
	reset();
	metadata = my_metadata;
	metadata.header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA, 0)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, false, false)
			== LZMA_DATA_ERROR);

	// Header Metadata is present but Size of Header Metadata is missing
	// from Footer.
	reset();
	metadata = my_metadata;
	metadata.header_metadata_size = LZMA_VLI_VALUE_UNKNOWN;
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA,
			my_metadata.header_metadata_size) == LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &metadata, false, false)
			== LZMA_DATA_ERROR);

	// Index missing
	reset();
	metadata = my_metadata;
	metadata.index = NULL;
	expect(lzma_info_metadata_set(info, NULL, &metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &metadata, false, false)
			== LZMA_DATA_ERROR);

	// Index in Header Metadata but not in Footer Metadata
	reset();
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &metadata, false, false)
			== LZMA_OK);

	// Index in Header Metadata but not in Footer Metadata but
	// Total Size is missing from Footer.
	reset();
	metadata.total_size = LZMA_VLI_VALUE_UNKNOWN;
	expect(lzma_info_metadata_set(info, NULL, &my_metadata, true, false)
			== LZMA_OK);
	expect(lzma_info_metadata_set(info, NULL, &metadata, false, false)
			== LZMA_DATA_ERROR);

	// Total Size doesn't match the Index
	reset();
	metadata = my_metadata;
	metadata.total_size = 9999;
	expect(lzma_info_metadata_set(info, NULL, &metadata, true, false)
			== LZMA_DATA_ERROR);

	// Uncompressed Size doesn't match the Index
	reset();
	metadata = my_metadata;
	metadata.uncompressed_size = 9999;
	expect(lzma_info_metadata_set(info, NULL, &metadata, true, false)
			== LZMA_DATA_ERROR);

	reset();
}


static void
test7(void)
{
	// No info yet, so we cannot locate anything.
	expect(lzma_info_metadata_locate(info, true)
			== LZMA_VLI_VALUE_UNKNOWN);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	// Setting the Stream start offset doesn't change this situation.
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START, 5) == LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== LZMA_VLI_VALUE_UNKNOWN);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	// Setting the Size of Header Metadata known allows us to locate
	// the Header Metadata Block.
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA, 11)
			== LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== 5 + LZMA_STREAM_HEADER_SIZE);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	// Adding a Data Block. As long as Index is not Finished, we cannot
	// locate Footer Metadata Block.
	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_iter_next(&iter, NULL) == LZMA_OK);
	expect(iter.stream_offset == 5 + LZMA_STREAM_HEADER_SIZE + 11);
	expect(iter.uncompressed_offset == 0);
	expect(lzma_info_iter_set(&iter, 22, 33) == LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== 5 + LZMA_STREAM_HEADER_SIZE);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	// Once the Index is finished, we can locate Footer Metadata Block too.
	expect(lzma_info_index_finish(info) == LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== 5 + LZMA_STREAM_HEADER_SIZE);
	expect(lzma_info_metadata_locate(info, false)
			== 5 + LZMA_STREAM_HEADER_SIZE + 11 + 22);

	// A retry of most of the above but now with unknown Size of Header
	// Metadata Block, which makes locating Footer Metadata Block
	// impossible.
	reset();
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START, 5) == LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== LZMA_VLI_VALUE_UNKNOWN);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	expect(lzma_info_index_set(info, NULL, my_index, false) == LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== LZMA_VLI_VALUE_UNKNOWN);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_VLI_VALUE_UNKNOWN);

	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA, 11)
			== LZMA_OK);
	expect(lzma_info_metadata_locate(info, true)
			== 5 + LZMA_STREAM_HEADER_SIZE);
	expect(lzma_info_metadata_locate(info, false)
			== LZMA_STREAM_HEADER_SIZE + 5 + 11 + 22 + 44 + 66);

	reset();
}


static void
test8(void)
{
	expect(lzma_info_size_set(info, LZMA_INFO_STREAM_START, 5) == LZMA_OK);
	expect(lzma_info_size_set(info, LZMA_INFO_HEADER_METADATA, 11)
			== LZMA_OK);

	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_iter_locate(&iter, NULL, 0, false)
			== LZMA_DATA_ERROR);
	expect(lzma_info_index_count_get(info) == 0);

	lzma_info_iter_begin(info, &iter);
	expect(lzma_info_iter_locate(&iter, NULL, 0, true) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);
	expect(lzma_info_iter_locate(&iter, NULL, 0, false) == LZMA_OK);
	expect(lzma_info_index_count_get(info) == 1);

	// TODO
}


/*
static void
test9(void)
{
	// TODO Various integer overflow checks
}
*/


int
main(void)
{
	lzma_init();

	info = lzma_info_init(NULL, NULL);
	if (info == NULL)
		return 1;

	validate();

	test1();
	test2();
	test3();
	test4();
	test5();
	test6();
	test7();
	test8();

	lzma_info_free(info, NULL);
	return 0;
}
