///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_inndex.c
/// \brief      Tests functions handling the lzma_index structure
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


int
main()
{
	lzma_index index[3] = {
		{ 22, 33, index + 1 },
		{ 44, 55, index + 2 },
		{ 66, 77, NULL },
	};

	lzma_index *i = lzma_index_dup(index, NULL);
	expect(i != NULL);

	expect(lzma_index_is_equal(index, i));

	i->next->next->uncompressed_size = 99;
	expect(!lzma_index_is_equal(index, i));

	lzma_index_free(i, NULL);

	return 0;
}
