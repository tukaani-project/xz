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


/*
#define pow2(e) (UINT32_C(1) << (e))


static const lzma_options_lzma presets[9] = {
// dict           lc lp pb           mode              fb   mf          mfc
{ pow2(16), NULL, 0, 3, 0, 2, false, LZMA_MODE_FAST,    64, LZMA_MF_HC3, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(20), NULL, 0, 3, 0, 0, false, LZMA_MODE_FAST,    64, LZMA_MF_HC4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(19), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL,  64, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(20), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL,  64, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(21), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL, 128, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(22), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL, 128, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(23), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL, 128, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(24), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL, 273, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
{ pow2(25), NULL, 0, 3, 0, 0, false, LZMA_MODE_NORMAL, 273, LZMA_MF_BT4, 0, 0, 0, 0, 0, NULL, NULL },
};


extern LZMA_API lzma_bool
lzma_lzma_preset(lzma_options_lzma *options, uint32_t level)
{
	if (level >= ARRAY_SIZE(presetes))
		return true;

	*options = presets[level];
	return false;
}
*/


extern LZMA_API lzma_bool
lzma_lzma_preset(lzma_options_lzma *options, uint32_t level)
{
	if (level == 0 || level > 9)
		return true;

	--level;
	memzero(options, sizeof(*options));

	static const uint8_t shift[9] = { 16, 20, 19, 20, 21, 22, 23, 24, 25 };
	options->dict_size = UINT32_C(1) << shift[level];

	options->preset_dict = NULL;
	options->preset_dict_size = 0;

	options->lc = LZMA_LC_DEFAULT;
	options->lp = LZMA_LP_DEFAULT;
	options->pb = LZMA_PB_DEFAULT;

	options->persistent = false;
	options->mode = level <= 2 ? LZMA_MODE_FAST : LZMA_MODE_NORMAL;

	options->nice_len = level <= 5 ? 32 : 64;
	options->mf = level <= 1 ? LZMA_MF_HC3 : level <= 2 ? LZMA_MF_HC4
			: LZMA_MF_BT4;
	options->depth = 0;

	return false;
}
