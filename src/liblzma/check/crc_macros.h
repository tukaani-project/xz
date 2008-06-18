///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc_macros
/// \brief      Some endian-dependent macros for CRC32 and CRC64
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifdef WORDS_BIGENDIAN
#	include "../../common/bswap.h"

#	define A(x) ((x) >> 24)
#	define B(x) (((x) >> 16) & 0xFF)
#	define C(x) (((x) >> 8) & 0xFF)
#	define D(x) ((x) & 0xFF)

#	define S8(x) ((x) << 8)
#	define S32(x) ((x) << 32)

#else
#	define A(x) ((x) & 0xFF)
#	define B(x) (((x) >> 8) & 0xFF)
#	define C(x) (((x) >> 16) & 0xFF)
#	define D(x) ((x) >> 24)

#	define S8(x) ((x) >> 8)
#	define S32(x) ((x) >> 32)
#endif
