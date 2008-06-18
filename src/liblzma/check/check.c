///////////////////////////////////////////////////////////////////////////////
//
/// \file       check.c
/// \brief      Check sizes
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"

// See the .lzma header format specification section 2.1.1.2.
LZMA_API const uint32_t lzma_check_sizes[LZMA_CHECK_ID_MAX + 1] = {
	0,
	4, 4, 4,
	8, 8, 8,
	16, 16, 16,
	32, 32, 32,
	64, 64, 64
};


LZMA_API const lzma_bool lzma_available_checks[LZMA_CHECK_ID_MAX + 1] = {
	true,   // LZMA_CHECK_NONE

#ifdef HAVE_CHECK_CRC32
	true,
#else
	false,
#endif

	false,  // Reserved
	false,  // Reserved

#ifdef HAVE_CHECK_CRC64
	true,
#else
	false,
#endif

	false,  // Reserved
	false,  // Reserved
	false,  // Reserved
	false,  // Reserved
	false,  // Reserved

#ifdef HAVE_CHECK_SHA256
	true,
#else
	false,
#endif

	false,  // Reserved
	false,  // Reserved
	false,  // Reserved
	false,  // Reserved
	false,  // Reserved
};


extern lzma_ret
lzma_check_init(lzma_check *check, lzma_check_type type)
{
	lzma_ret ret = LZMA_OK;

	switch (type) {
	case LZMA_CHECK_NONE:
		break;

#ifdef HAVE_CHECK_CRC32
	case LZMA_CHECK_CRC32:
		check->state.crc32 = 0;
		break;
#endif

#ifdef HAVE_CHECK_CRC64
	case LZMA_CHECK_CRC64:
		check->state.crc64 = 0;
		break;
#endif

#ifdef HAVE_CHECK_SHA256
	case LZMA_CHECK_SHA256:
		lzma_sha256_init(check);
		break;
#endif

	default:
		if ((unsigned)(type) <= LZMA_CHECK_ID_MAX)
			ret = LZMA_UNSUPPORTED_CHECK;
		else
			ret = LZMA_PROG_ERROR;
		break;
	}

	return ret;
}


extern void
lzma_check_update(lzma_check *check, lzma_check_type type,
		const uint8_t *buf, size_t size)
{
	switch (type) {
#ifdef HAVE_CHECK_CRC32
	case LZMA_CHECK_CRC32:
		check->state.crc32 = lzma_crc32(buf, size, check->state.crc32);
		break;
#endif

#ifdef HAVE_CHECK_CRC64
	case LZMA_CHECK_CRC64:
		check->state.crc64 = lzma_crc64(buf, size, check->state.crc64);
		break;
#endif

#ifdef HAVE_CHECK_SHA256
	case LZMA_CHECK_SHA256:
		lzma_sha256_update(buf, size, check);
		break;
#endif

	default:
		break;
	}

	return;
}


extern void
lzma_check_finish(lzma_check *check, lzma_check_type type)
{
	switch (type) {
#ifdef HAVE_CHECK_CRC32
	case LZMA_CHECK_CRC32:
		*(uint32_t *)(check->buffer) = check->state.crc32;
		break;
#endif

#ifdef HAVE_CHECK_CRC64
	case LZMA_CHECK_CRC64:
		*(uint64_t *)(check->buffer) = check->state.crc64;
		break;
#endif

#ifdef HAVE_CHECK_SHA256
	case LZMA_CHECK_SHA256:
		lzma_sha256_finish(check);
		break;
#endif

	default:
		break;
	}

	return;
}


/*
extern bool
lzma_check_compare(
		lzma_check *check1, lzma_check *check2, lzma_check_type type)
{
	bool ret;

	switch (type) {
	case LZMA_CHECK_NONE:
		break;

	case LZMA_CHECK_CRC32:
		ret = check1->crc32 != check2->crc32;
		break;

	case LZMA_CHECK_CRC64:
		ret = check1->crc64 != check2->crc64;
		break;

	default:
		// Unsupported check
		assert(type <= 7);
		ret = false;
		break;
	}

	return ret;
}
*/
