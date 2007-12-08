///////////////////////////////////////////////////////////////////////////////
//
/// \file       suffix.c
/// \brief      Checks filename suffix and creates the destination filename
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

#include "private.h"


static const struct {
	const char *compressed;
	const char *uncompressed;
} suffixes[] = {
	{ ".lzma",  "" },
	{ ".tlz",   ".tar" },
	{ ".ylz",   ".yar" },
	{ NULL,     NULL }
};


/// \brief      Checks if src_name has given compressed_suffix
///
/// \param      suffix      Filename suffix to look for
/// \param      src_name    Input filename
/// \param      src_len     strlen(src_name)
///
/// \return     If src_name has the suffix, src_len - strlen(suffix) is
///             returned. It's always a positive integer. Otherwise zero
///             is returned.
static size_t
test_suffix(const char *suffix, const char *src_name, size_t src_len)
{
	const size_t suffix_len = strlen(suffix);

	// The filename must have at least one character in addition to
	// the suffix. src_name may contain path to the filename, so we
	// need to check for directory separator too.
	if (src_len <= suffix_len || src_name[src_len - suffix_len - 1] == '/')
		return 0;

	if (strcmp(suffix, src_name + src_len - suffix_len) == 0)
		return src_len - suffix_len;

	return 0;
}


/// \brief      Removes the filename suffix of the compressed file
///
/// \return     Name of the uncompressed file, or NULL if file has unknown
///             suffix.
static char *
uncompressed_name(const char *src_name)
{
	const char *new_suffix = "";
	const size_t src_len = strlen(src_name);
	size_t new_len = 0;

	for (size_t i = 0; suffixes[i].compressed != NULL; ++i) {
		new_len = test_suffix(suffixes[i].compressed,
				src_name, src_len);
		if (new_len != 0) {
			new_suffix = suffixes[i].uncompressed;
			break;
		}
	}

	if (new_len == 0 && opt_suffix != NULL)
		new_len = test_suffix(opt_suffix, src_name, src_len);

	if (new_len == 0) {
		errmsg(V_WARNING, _("%s: Filename has an unknown suffix, "
				"skipping"), src_name);
		return NULL;
	}

	const size_t new_suffix_len = strlen(new_suffix);
	char *dest_name = malloc(new_len + new_suffix_len + 1);
	if (dest_name == NULL) {
		out_of_memory();
		return NULL;
	}

	memcpy(dest_name, src_name, new_len);
	memcpy(dest_name + new_len, new_suffix, new_suffix_len);
	dest_name[new_len + new_suffix_len] = '\0';

	return dest_name;
}


/// \brief      Appends suffix to src_name
static char *
compressed_name(const char *src_name)
{
	const size_t src_len = strlen(src_name);

	for (size_t i = 0; suffixes[i].compressed != NULL; ++i) {
		if (test_suffix(suffixes[i].compressed, src_name, src_len)
				!= 0) {
			errmsg(V_WARNING, _("%s: File already has `%s' "
					"suffix, skipping"), src_name,
					suffixes[i].compressed);
			return NULL;
		}
	}

	const char *suffix = opt_suffix != NULL
			? opt_suffix : suffixes[0].compressed;
	const size_t suffix_len = strlen(suffix);

	char *dest_name = malloc(src_len + suffix_len + 1);
	if (dest_name == NULL) {
		out_of_memory();
		return NULL;
	}

	memcpy(dest_name, src_name, src_len);
	memcpy(dest_name + src_len, suffix, suffix_len);
	dest_name[src_len + suffix_len] = '\0';

	return dest_name;
}


extern char *
get_dest_name(const char *src_name)
{
	return opt_mode == MODE_COMPRESS
			? compressed_name(src_name)
			: uncompressed_name(src_name);
}
