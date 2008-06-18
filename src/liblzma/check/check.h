///////////////////////////////////////////////////////////////////////////////
//
/// \file       check.h
/// \brief      Prototypes for different check functions
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_CHECK_H
#define LZMA_CHECK_H

#include "common.h"


// Index hashing used to verify the Index with O(1) memory usage needs
// a good hash function.
#if defined(HAVE_CHECK_SHA256)
#	define LZMA_CHECK_BEST LZMA_CHECK_SHA256
#elif defined(HAVE_CHECK_CRC64)
#	define LZMA_CHECK_BEST LZMA_CHECK_CRC64
#else
#	define LZMA_CHECK_BEST LZMA_CHECK_CRC32
#endif


typedef struct {
	/// Internal state
	uint32_t state[8];

	/// Size of the message excluding padding
	uint64_t size;

} lzma_sha256;


/// \note       This is not in the public API because this structure will
///             change in future.
typedef struct {
	// FIXME Guarantee 8-byte alignment

	/// Buffer to hold the final result; this is also used as a temporary
	/// buffer in SHA256. Note that this buffer must be 8-byte aligned.
	uint8_t buffer[64];

	/// Check-specific data
	union {
		uint32_t crc32;
		uint64_t crc64;

		struct {
			/// Internal state
			uint32_t state[8];

			/// Size of the message excluding padding
			uint64_t size;
		} sha256;
	} state;

} lzma_check;


#ifdef HAVE_SMALL
extern uint32_t lzma_crc32_table[8][256];
extern uint64_t lzma_crc64_table[4][256];
#else
extern const uint32_t lzma_crc32_table[8][256];
extern const uint64_t lzma_crc64_table[4][256];
#endif

// Generic

/// \brief      Initializes *check depending on type
///
/// \return     LZMA_OK on success. LZMA_UNSUPPORTED_CHECK if the type is not
///             supported by the current version or build of liblzma.
///             LZMA_PROG_ERROR if type > LZMA_CHECK_ID_MAX.
///
extern lzma_ret lzma_check_init(lzma_check *check, lzma_check_type type);

/// \brief      Updates *check
///
extern void lzma_check_update(lzma_check *check, lzma_check_type type,
		const uint8_t *buf, size_t size);

/// \brief      Finishes *check
///
extern void lzma_check_finish(lzma_check *check, lzma_check_type type);


/*
/// \brief      Compare two checks
///
/// \return     false if the checks are identical; true if they differ.
///
extern bool lzma_check_compare(
		lzma_check *check1, lzma_check *check2, lzma_check_type type);
*/


// CRC32

extern void lzma_crc32_init(void);


// CRC64

extern void lzma_crc64_init(void);


// SHA256

extern void lzma_sha256_init(lzma_check *check);

extern void lzma_sha256_update(
		const uint8_t *buf, size_t size, lzma_check *check);

extern void lzma_sha256_finish(lzma_check *check);


#endif
