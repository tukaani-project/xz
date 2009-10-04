///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_integer.h
/// \brief      Byte swapping and endianness related macros and functions
///
/// This file provides macros or functions to do basic endianness related
/// integer operations (XX = 16, 32, or 64; Y = b or l):
///   - Byte swapping: bswapXX(num)
///   - Byte order conversions to/from native: convXXYe(num)
///   - Aligned reads: readXXYe(ptr)
///   - Aligned writes: writeXXYe(ptr, num)
///   - Unaligned reads (16/32-bit only): unaligned_readXXYe(ptr)
///   - Unaligned writes (16/32-bit only): unaligned_writeXXYe(ptr, num)
///
/// Since they can macros, the arguments should have no side effects since
/// they may be evaluated more than once.
///
/// \todo       PowerPC and possibly some other architectures support
///             byte swapping load and store instructions. This file
///             doesn't take advantage of those instructions.
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TUKLIB_INTEGER_H
#define TUKLIB_INTEGER_H

#include "tuklib_common.h"


////////////////////////////////////////
// Operating system specific features //
////////////////////////////////////////

#if defined(HAVE_BYTESWAP_H)
	// glibc, uClibc, dietlibc
#	include <byteswap.h>
#	ifdef HAVE_BSWAP_16
#		define bswap16(num) bswap_16(num)
#	endif
#	ifdef HAVE_BSWAP_32
#		define bswap32(num) bswap_32(num)
#	endif
#	ifdef HAVE_BSWAP_64
#		define bswap64(num) bswap_64(num)
#	endif

#elif defined(HAVE_SYS_ENDIAN_H)
	// *BSDs and Darwin
#	include <sys/endian.h>

#elif defined(HAVE_SYS_BYTEORDER_H)
	// Solaris
#	include <sys/byteorder.h>
#	ifdef BSWAP_16
#		define bswap16(num) BSWAP_16(num)
#	endif
#	ifdef BSWAP_32
#		define bswap32(num) BSWAP_32(num)
#	endif
#	ifdef BSWAP_64
#		define bswap64(num) BSWAP_64(num)
#	endif
#	ifdef BE_16
#		define conv16be(num) BE_16(num)
#	endif
#	ifdef BE_32
#		define conv32be(num) BE_32(num)
#	endif
#	ifdef BE_64
#		define conv64be(num) BE_64(num)
#	endif
#	ifdef LE_16
#		define conv16le(num) LE_16(num)
#	endif
#	ifdef LE_32
#		define conv32le(num) LE_32(num)
#	endif
#	ifdef LE_64
#		define conv64le(num) LE_64(num)
#	endif
#endif


///////////////////
// Byte swapping //
///////////////////

#ifndef bswap16
#	define bswap16(num) \
		(((uint16_t)(num) << 8) | ((uint16_t)(num) >> 8))
#endif

#ifndef bswap32
#	define bswap32(num) \
		( (((uint32_t)(num) << 24)                       ) \
		| (((uint32_t)(num) <<  8) & UINT32_C(0x00FF0000)) \
		| (((uint32_t)(num) >>  8) & UINT32_C(0x0000FF00)) \
		| (((uint32_t)(num) >> 24)                       ) )
#endif

#ifndef bswap64
#	define bswap64(num) \
		( (((uint64_t)(num) << 56)                               ) \
		| (((uint64_t)(num) << 40) & UINT64_C(0x00FF000000000000)) \
		| (((uint64_t)(num) << 24) & UINT64_C(0x0000FF0000000000)) \
		| (((uint64_t)(num) <<  8) & UINT64_C(0x000000FF00000000)) \
		| (((uint64_t)(num) >>  8) & UINT64_C(0x00000000FF000000)) \
		| (((uint64_t)(num) >> 24) & UINT64_C(0x0000000000FF0000)) \
		| (((uint64_t)(num) >> 40) & UINT64_C(0x000000000000FF00)) \
		| (((uint64_t)(num) >> 56)                               ) )
#endif

// Define conversion macros using the basic byte swapping macros.
#ifdef WORDS_BIGENDIAN
#	ifndef conv16be
#		define conv16be(num) ((uint16_t)(num))
#	endif
#	ifndef conv32be
#		define conv32be(num) ((uint32_t)(num))
#	endif
#	ifndef conv64be
#		define conv64be(num) ((uint64_t)(num))
#	endif
#	ifndef conv16le
#		define conv16le(num) bswap16(num)
#	endif
#	ifndef conv32le
#		define conv32le(num) bswap32(num)
#	endif
#	ifndef conv64le
#		define conv64le(num) bswap64(num)
#	endif
#else
#	ifndef conv16be
#		define conv16be(num) bswap16(num)
#	endif
#	ifndef conv32be
#		define conv32be(num) bswap32(num)
#	endif
#	ifndef conv64be
#		define conv64be(num) bswap64(num)
#	endif
#	ifndef conv16le
#		define conv16le(num) ((uint16_t)(num))
#	endif
#	ifndef conv32le
#		define conv32le(num) ((uint32_t)(num))
#	endif
#	ifndef conv64le
#		define conv64le(num) ((uint64_t)(num))
#	endif
#endif


//////////////////////////////
// Aligned reads and writes //
//////////////////////////////

static inline uint16_t
read16be(const uint8_t *buf)
{
	uint16_t num = *(const uint16_t *)buf;
	return conv16be(num);
}


static inline uint16_t
read16le(const uint8_t *buf)
{
	uint16_t num = *(const uint16_t *)buf;
	return conv16le(num);
}


static inline uint32_t
read32be(const uint8_t *buf)
{
	uint32_t num = *(const uint32_t *)buf;
	return conv32be(num);
}


static inline uint32_t
read32le(const uint8_t *buf)
{
	uint32_t num = *(const uint32_t *)buf;
	return conv32le(num);
}


static inline uint64_t
read64be(const uint8_t *buf)
{
	uint64_t num = *(const uint64_t *)buf;
	return conv64be(num);
}


static inline uint64_t
read64le(const uint8_t *buf)
{
	uint64_t num = *(const uint64_t *)buf;
	return conv64le(num);
}


// NOTE: Possible byte swapping must be done in a macro to allow GCC
// to optimize byte swapping of constants when using glibc's or *BSD's
// byte swapping macros. The actual write is done in an inline function
// to make type checking of the buf pointer possible similarly to readXXYe()
// functions. This also seems to silence a probably bogus GCC warning about
// strict aliasing when buf points to the beginning of an uint8_t array.

#define write16be(buf, num) write16ne((buf), conv16be(num))
#define write16le(buf, num) write16ne((buf), conv16le(num))
#define write32be(buf, num) write32ne((buf), conv32be(num))
#define write32le(buf, num) write32ne((buf), conv32le(num))
#define write64be(buf, num) write64ne((buf), conv64be(num))
#define write64le(buf, num) write64ne((buf), conv64le(num))


static inline void
write16ne(uint8_t *buf, uint16_t num)
{
	*(uint16_t *)buf = num;
	return;
}


static inline void
write32ne(uint8_t *buf, uint32_t num)
{
	*(uint32_t *)buf = num;
	return;
}


static inline void
write64ne(uint8_t *buf, uint64_t num)
{
	*(uint64_t *)buf = num;
	return;
}


////////////////////////////////
// Unaligned reads and writes //
////////////////////////////////

// NOTE: TUKLIB_FAST_UNALIGNED_ACCESS indicates only support for 16-bit and
// 32-bit unaligned integer loads and stores. It's possible that 64-bit
// unaligned access doesn't work or is slower than byte-by-byte access.
// Since unaligned 64-bit is probably not needed as often as 16-bit or
// 32-bit, we simply don't support 64-bit unaligned access for now.
#ifdef TUKLIB_FAST_UNALIGNED_ACCESS
#	define unaligned_read16be read16be
#	define unaligned_read16le read16le
#	define unaligned_read32be read32be
#	define unaligned_read32le read32le
#	define unaligned_write16be write16be
#	define unaligned_write16le write16le
#	define unaligned_write32be write32be
#	define unaligned_write32le write32le

#else

static inline uint16_t
unaligned_read16be(const uint8_t *buf)
{
	uint16_t num = ((uint16_t)buf[0] << 8) | buf[1];
	return num;
}


static inline uint16_t
unaligned_read16le(const uint8_t *buf)
{
	uint16_t num = ((uint32_t)buf[0]) | ((uint16_t)buf[1] << 8);
	return num;
}


static inline uint32_t
unaligned_read32be(const uint8_t *buf)
{
	uint32_t num = (uint32_t)buf[0] << 24;
	num |= (uint32_t)buf[1] << 16;
	num |= (uint32_t)buf[2] << 8;
	num |= (uint32_t)buf[3];
	return num;
}


static inline uint32_t
unaligned_read32le(const uint8_t *buf)
{
	uint32_t num = (uint32_t)buf[0];
	num |= (uint32_t)buf[1] << 8;
	num |= (uint32_t)buf[2] << 16;
	num |= (uint32_t)buf[3] << 24;
	return num;
}


static inline void
unaligned_write16be(uint8_t *buf, uint16_t num)
{
	buf[0] = num >> 8;
	buf[1] = num;
	return;
}


static inline void
unaligned_write16le(uint8_t *buf, uint16_t num)
{
	buf[0] = num;
	buf[1] = num >> 8;
	return;
}


static inline void
unaligned_write32be(uint8_t *buf, uint32_t num)
{
	buf[0] = num >> 24;
	buf[1] = num >> 16;
	buf[2] = num >> 8;
	buf[3] = num;
	return;
}


static inline void
unaligned_write32le(uint8_t *buf, uint32_t num)
{
	buf[0] = num;
	buf[1] = num >> 8;
	buf[2] = num >> 16;
	buf[3] = num >> 24;
	return;
}

#endif
#endif
