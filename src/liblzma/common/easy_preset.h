///////////////////////////////////////////////////////////////////////////////
//
/// \file       easy_preset.h
/// \brief      Preset handling for easy encoder and decoder
//
//  Copyright (C) 2009 Lasse Collin
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


typedef struct {
	/// We need to keep the filters array available in case
	/// LZMA_FULL_FLUSH is used.
	lzma_filter filters[LZMA_FILTERS_MAX + 1];

	/// Options for LZMA2
	lzma_options_lzma opt_lzma;

	// Options for more filters can be added later, so this struct
	// is not ready to be put into the public API.

} lzma_options_easy;


/// Set *easy to the settings given by the preset. Returns true on error,
/// false on success.
extern bool lzma_easy_preset(lzma_options_easy *easy, uint32_t preset);
