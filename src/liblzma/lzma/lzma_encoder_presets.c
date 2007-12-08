///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_presets.c
/// \brief      Encoder presets
//
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


LZMA_API const lzma_options_lzma lzma_preset_lzma[9] = {
// dictionary_size  lc lp pb           mode            fb  mf          mfc
{ UINT32_C(1) << 16, 3, 0, 2, NULL, 0, LZMA_MODE_FAST,  64, LZMA_MF_HC3, 0 },
{ UINT32_C(1) << 20, 3, 0, 2, NULL, 0, LZMA_MODE_FAST,  64, LZMA_MF_HC4, 0 },
{ UINT32_C(1) << 19, 3, 0, 2, NULL, 0, LZMA_MODE_BEST,  64, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 20, 3, 0, 2, NULL, 0, LZMA_MODE_BEST,  64, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 21, 3, 0, 2, NULL, 0, LZMA_MODE_BEST, 128, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 22, 3, 0, 2, NULL, 0, LZMA_MODE_BEST, 128, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 23, 3, 0, 2, NULL, 0, LZMA_MODE_BEST, 128, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 24, 3, 0, 2, NULL, 0, LZMA_MODE_BEST, 273, LZMA_MF_BT4, 0 },
{ UINT32_C(1) << 25, 3, 0, 2, NULL, 0, LZMA_MODE_BEST, 273, LZMA_MF_BT4, 0 },
};
