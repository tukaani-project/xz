///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_memlimit.c
/// \brief      Tests the memory usage limitter
///
/// \note       These tests cannot be done at exact byte count accuracy,
///             because memory limitter takes into account the memory wasted
///             by bookkeeping structures and alignment (padding).
//
//  Copyright (C) 2008 Lasse Collin
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
main(void)
{
	void *a;
	void *b;
	lzma_memlimit *mem;

	expect((mem = lzma_memlimit_create(1 << 16)) != NULL);
	expect(lzma_memlimit_count(mem) == 0);
	expect(lzma_memlimit_used(mem) > 0);
	expect(lzma_memlimit_used(mem) < 4096);
	expect(lzma_memlimit_used(mem) == lzma_memlimit_max(mem, false));
	expect(!lzma_memlimit_reached(mem, false));

	expect((a = lzma_memlimit_alloc(mem, 1, 4096)) != NULL);
	expect(lzma_memlimit_count(mem) == 1);
	expect(lzma_memlimit_used(mem) > 4096);
	expect(lzma_memlimit_used(mem) < 8192);
	expect(lzma_memlimit_used(mem) == lzma_memlimit_max(mem, false));
	expect(!lzma_memlimit_reached(mem, false));

	expect((b = lzma_memlimit_alloc(mem, 1, 4096)) != NULL);
	expect(lzma_memlimit_count(mem) == 2);
	expect(lzma_memlimit_used(mem) > 8192);
	expect(lzma_memlimit_used(mem) < 12288);
	expect(lzma_memlimit_used(mem) == lzma_memlimit_max(mem, false));
	expect(!lzma_memlimit_reached(mem, false));

	expect((lzma_memlimit_alloc(mem, 1, 1 << 17)) == NULL);
	expect(lzma_memlimit_count(mem) == 2);
	expect(lzma_memlimit_used(mem) > 8192);
	expect(lzma_memlimit_used(mem) < 12288);
	expect(lzma_memlimit_used(mem) < lzma_memlimit_max(mem, false));
	expect(lzma_memlimit_max(mem, false) > (1 << 17));
	expect(lzma_memlimit_reached(mem, false));

	lzma_memlimit_free(mem, a);
	expect(lzma_memlimit_count(mem) == 1);
	expect(lzma_memlimit_used(mem) > 4096);
	expect(lzma_memlimit_used(mem) < 8192);
	expect(lzma_memlimit_max(mem, true) > (1 << 17));
	expect(lzma_memlimit_reached(mem, true));
	expect(lzma_memlimit_used(mem) == lzma_memlimit_max(mem, false));
	expect(!lzma_memlimit_reached(mem, false));

	expect(lzma_memlimit_get(mem) == 1 << 16);
	lzma_memlimit_set(mem, 6144);
	expect(lzma_memlimit_get(mem) == 6144);
	expect(lzma_memlimit_alloc(mem, 1, 4096) == NULL);
	expect(lzma_memlimit_max(mem, false) > 8192);
	expect(lzma_memlimit_reached(mem, false));

	lzma_memlimit_free(mem, b);
	expect(lzma_memlimit_count(mem) == 0);
	expect(lzma_memlimit_used(mem) > 0);
	expect(lzma_memlimit_used(mem) < 4096);

	expect((a = lzma_memlimit_alloc(mem, 1, 4096)) != NULL);
	expect(lzma_memlimit_count(mem) == 1);
	expect(lzma_memlimit_used(mem) > 4096);
	expect(lzma_memlimit_used(mem) < 8192);

	expect(lzma_memlimit_max(mem, false) > 8192);
	expect(lzma_memlimit_reached(mem, false));
	expect(lzma_memlimit_max(mem, true) > 8192);
	expect(lzma_memlimit_reached(mem, true));
	expect(lzma_memlimit_max(mem, true) < 8192);
	expect(!lzma_memlimit_reached(mem, true));

	lzma_memlimit_detach(mem, a);
	free(a);
	expect(lzma_memlimit_count(mem) == 0);

	lzma_memlimit_set(mem, SIZE_MAX);
	expect(lzma_memlimit_alloc(mem, 1, SIZE_MAX - 33) == NULL);
	expect(lzma_memlimit_count(mem) == 0);
	expect(lzma_memlimit_max(mem, true) == SIZE_MAX);
	expect(lzma_memlimit_reached(mem, true));

	expect(lzma_memlimit_alloc(mem, 1, SIZE_MAX) == NULL);
	expect(lzma_memlimit_count(mem) == 0);
	expect(lzma_memlimit_max(mem, false) == SIZE_MAX);
	expect(lzma_memlimit_reached(mem, false));

	lzma_memlimit_end(mem, true);

	return 0;
}
