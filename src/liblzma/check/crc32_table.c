///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_table.c
/// \brief      Precalculated CRC32 table with correct endianness
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


// FIXME: Compared to crc32_fast.c this has to check for __x86_64__ too
// so that in 32-bit builds crc32_x86.S won't break due to a missing table.
#if !defined(HAVE_ENCODERS) && ((defined(__x86_64__) && defined(__SSSE3__) \
			&& defined(__SSE4_1__) && defined(__PCLMUL__)) \
			|| (defined(__e2k__) && __iset__ >= 6))
// No table needed. Use a typedef to avoid an empty translation unit.
typedef void lzma_crc32_dummy;

#else
// Having the declaration here silences clang -Wmissing-variable-declarations.
extern const uint32_t lzma_crc32_table[8][256];

#       ifdef WORDS_BIGENDIAN
#       	include "crc32_table_be.h"
#       else
#       	include "crc32_table_le.h"
#       endif
#endif
