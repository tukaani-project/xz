///////////////////////////////////////////////////////////////////////////////
//
/// \file       options.h
/// \brief      Parser for filter-specific options
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      Parser for Subblock options
///
/// \return     Pointer to allocated options structure.
///             Doesn't return on error.
extern lzma_options_subblock *options_subblock(const char *str);


/// \brief      Parser for Delta options
///
/// \return     Pointer to allocated options structure.
///             Doesn't return on error.
extern lzma_options_delta *options_delta(const char *str);


/// \brief      Parser for LZMA options
///
/// \return     Pointer to allocated options structure.
///             Doesn't return on error.
extern lzma_options_lzma *options_lzma(const char *str);
