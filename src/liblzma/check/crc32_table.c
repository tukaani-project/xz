///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_table.c
/// \brief      Precalculated CRC32 table with correct endianness
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"

#ifdef WORDS_BIGENDIAN
#	include "crc32_table_be.h"
#else
#	include "crc32_table_le.h"
#endif
