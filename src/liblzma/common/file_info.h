// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       file_info.h
/// \brief      Decode .xz file information into a lzma_index structure
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_FILE_INFO_H
#define LZMA_FILE_INFO_H

#include "common.h"

extern lzma_ret lzma_file_info_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator, uint64_t *seek_pos,
		lzma_index **dest_index,
		uint64_t memlimit, uint64_t file_size);

#endif
