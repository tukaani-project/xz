///////////////////////////////////////////////////////////////////////////////
//
/// \file       bswap.h
/// \brief      Byte swapping
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_BSWAP_H
#define LZMA_BSWAP_H

// NOTE: We assume that config.h is already #included.

// byteswap.h is a GNU extension. It contains inline assembly versions
// for byteswapping. When byteswap.h is not available, we use generic code.
#ifdef HAVE_BYTESWAP_H
#	include <byteswap.h>
#else
#	define bswap_16(num) \
		(((num) << 8) | ((num) >> 8))

#	define bswap_32(num) \
		( (((num) << 24)                       ) \
		| (((num) <<  8) & UINT32_C(0x00FF0000)) \
		| (((num) >>  8) & UINT32_C(0x0000FF00)) \
		| (((num) >> 24)                       ) )

#	define bswap_64(num) \
		( (((num) << 56)                               ) \
		| (((num) << 40) & UINT64_C(0x00FF000000000000)) \
		| (((num) << 24) & UINT64_C(0x0000FF0000000000)) \
		| (((num) <<  8) & UINT64_C(0x000000FF00000000)) \
		| (((num) >>  8) & UINT64_C(0x00000000FF000000)) \
		| (((num) >> 24) & UINT64_C(0x0000000000FF0000)) \
		| (((num) >> 40) & UINT64_C(0x000000000000FF00)) \
		| (((num) >> 56)                               ) )
#endif

#endif
