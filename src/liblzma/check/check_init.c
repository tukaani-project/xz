///////////////////////////////////////////////////////////////////////////////
//
/// \file       check_init.c
/// \brief      Static initializations for integrity checks
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"


extern LZMA_API void
lzma_init_check(void)
{
#ifdef HAVE_SMALL
	static bool already_initialized = false;
	if (already_initialized)
		return;

#	ifdef HAVE_CHECK_CRC32
	lzma_crc32_init();
#	endif

#	ifdef HAVE_CHECK_CRC64
	lzma_crc64_init();
#	endif

	already_initialized = true;
#endif

	return;
}
