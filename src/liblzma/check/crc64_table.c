///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_table.c
/// \brief      Precalculated CRC64 table with correct endianness
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef WORDS_BIGENDIAN
#	include "crc64_table_be.h"
#else
#	include "crc64_table_le.h"
#endif
