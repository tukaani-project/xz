// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder_hash.h
/// \brief      Hash macros for match finders
//
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZ_ENCODER_HASH_H
#define LZMA_LZ_ENCODER_HASH_H

// We need to know if CRC32_GENERIC is defined and we may need the declaration
// of lzma_crc32_table[][].
#include "crc_common.h"

// If HAVE_SMALL is defined, then lzma_crc32_table[][] exists and
// it's little endian even on big endian systems.
//
// If HAVE_SMALL isn't defined, lzma_crc32_table[][] is in native endian
// but we want a little endian one so that the compressed output won't
// depend on the processor endianness. Big endian systems are less common
// so those get the burden of an extra 1 KiB table.
//
// If HAVE_SMALL isn't defined and CRC32_GENERIC isn't defined either,
// then lzma_crc32_table[][] doesn't exist.
#if defined(HAVE_SMALL) \
		|| (defined(CRC32_GENERIC) && !defined(WORDS_BIGENDIAN))
#	define hash_table lzma_crc32_table[0]
#else
	// lz_encoder.c takes care of including the actual table.
	lzma_attr_visibility_hidden
	extern const uint32_t lzma_lz_hash_table[256];
#	define hash_table lzma_lz_hash_table
#	define LZMA_LZ_HASH_TABLE_IS_NEEDED 1
#endif

#define HASH_2_SIZE (UINT32_C(1) << 10)
#define HASH_3_SIZE (UINT32_C(1) << 16)
#define HASH_4_SIZE (UINT32_C(1) << 20)

#define HASH_2_MASK (HASH_2_SIZE - 1)
#define HASH_3_MASK (HASH_3_SIZE - 1)
#define HASH_4_MASK (HASH_4_SIZE - 1)

#define FIX_3_HASH_SIZE (HASH_2_SIZE)
#define FIX_4_HASH_SIZE (HASH_2_SIZE + HASH_3_SIZE)
#define FIX_5_HASH_SIZE (HASH_2_SIZE + HASH_3_SIZE + HASH_4_SIZE)

// Endianness doesn't matter in hash_2_calc() (no effect on the output).
#ifdef TUKLIB_FAST_UNALIGNED_ACCESS
#	define hash_2_calc() \
		const uint32_t hash_value = read16ne(cur)
#else
#	define hash_2_calc() \
		const uint32_t hash_value \
			= (uint32_t)(cur[0]) | ((uint32_t)(cur[1]) << 8)
#endif

#define hash_3_calc() \
	const uint32_t temp = hash_table[cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & mf->hash_mask

#define hash_4_calc() \
	const uint32_t temp = hash_table[cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	const uint32_t hash_value = (temp ^ ((uint32_t)(cur[2]) << 8) \
			^ (hash_table[cur[3]] << 5)) & mf->hash_mask

// Prefetch the buckets that hash_4_calc() will read for ptr. The indexes
// must match hash_4_calc(); if they drift apart the wrong lines are fetched
// and the only symptom is lost speed.
#define hash_4_prefetch(ptr) \
do { \
	const uint32_t pf_temp = hash_table[(ptr)[0]] ^ (ptr)[1]; \
	const uint32_t pf_h3 = pf_temp ^ ((uint32_t)((ptr)[2]) << 8); \
	lzma_prefetch(&mf->hash[pf_temp & HASH_2_MASK]); \
	lzma_prefetch(&mf->hash[FIX_3_HASH_SIZE + (pf_h3 & HASH_3_MASK)]); \
	lzma_prefetch(&mf->hash[FIX_4_HASH_SIZE \
			+ ((pf_h3 ^ (hash_table[(ptr)[3]] << 5)) \
				& mf->hash_mask)]); \
} while (0)


// The following are not currently used.

#define hash_5_calc() \
	const uint32_t temp = hash_table[cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	uint32_t hash_4_value = (temp ^ ((uint32_t)(cur[2]) << 8) ^ \
			^ hash_table[cur[3]] << 5); \
	const uint32_t hash_value \
			= (hash_4_value ^ (hash_table[cur[4]] << 3)) \
				& mf->hash_mask; \
	hash_4_value &= HASH_4_MASK

/*
#define hash_zip_calc() \
	const uint32_t hash_value \
			= (((uint32_t)(cur[0]) | ((uint32_t)(cur[1]) << 8)) \
				^ hash_table[cur[2]]) & 0xFFFF
*/

#define hash_zip_calc() \
	const uint32_t hash_value \
			= (((uint32_t)(cur[2]) | ((uint32_t)(cur[0]) << 8)) \
				^ hash_table[cur[1]]) & 0xFFFF

#define mt_hash_2_calc() \
	const uint32_t hash_2_value \
			= (hash_table[cur[0]] ^ cur[1]) & HASH_2_MASK

#define mt_hash_3_calc() \
	const uint32_t temp = hash_table[cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK

#define mt_hash_4_calc() \
	const uint32_t temp = hash_table[cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	const uint32_t hash_4_value = (temp ^ ((uint32_t)(cur[2]) << 8) ^ \
			(hash_table[cur[3]] << 5)) & HASH_4_MASK

#endif
