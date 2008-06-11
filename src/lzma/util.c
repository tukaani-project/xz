///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.c
/// \brief      Miscellaneous utility functions
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


/// \brief      Fancy version of strtoull()
///
/// \param      name    Name of the option to show in case of an error
/// \param      value   String containing the number to be parsed; may
///                     contain suffixes "k", "M", "G", "Ki", "Mi", or "Gi"
/// \param      min     Minimum valid value
/// \param      max     Maximum valid value
///
/// \return     Parsed value that is in the range [min, max]. Does not return
///             if an error occurs.
///
extern uint64_t
str_to_uint64(const char *name, const char *value, uint64_t min, uint64_t max)
{
	uint64_t result = 0;

	// Skip blanks.
	while (*value == ' ' || *value == '\t')
		++value;

	if (*value < '0' || *value > '9') {
		errmsg(V_ERROR, _("%s: Value is not a non-negative "
				"decimal integer"),
				value);
		my_exit(ERROR);
	}

	do {
		// Don't overflow.
		if (result > (UINT64_MAX - 9) / 10)
			goto error;

		result *= 10;
		result += *value - '0';
		++value;
	} while (*value >= '0' && *value <= '9');

	if (*value != '\0') {
		// Look for suffix.
		static const struct {
			const char name[4];
			uint64_t multiplier;
		} suffixes[] = {
			{ "k",   UINT64_C(1000) },
			{ "kB",  UINT64_C(1000) },
			{ "M",   UINT64_C(1000000) },
			{ "MB",  UINT64_C(1000000) },
			{ "G",   UINT64_C(1000000000) },
			{ "GB",  UINT64_C(1000000000) },
			{ "Ki",  UINT64_C(1024) },
			{ "KiB", UINT64_C(1024) },
			{ "Mi",  UINT64_C(1048576) },
			{ "MiB", UINT64_C(1048576) },
			{ "Gi",  UINT64_C(1073741824) },
			{ "GiB", UINT64_C(1073741824) }
		};

		uint64_t multiplier = 0;
		for (size_t i = 0; i < ARRAY_SIZE(suffixes); ++i) {
			if (strcmp(value, suffixes[i].name) == 0) {
				multiplier = suffixes[i].multiplier;
				break;
			}
		}

		if (multiplier == 0) {
			errmsg(V_ERROR, _("%s: Invalid multiplier suffix. "
					"Valid suffixes:"), value);
			errmsg(V_ERROR, "`k' (10^3), `M' (10^6), `G' (10^9) "
					"`Ki' (2^10), `Mi' (2^20), "
					"`Gi' (2^30)");
			my_exit(ERROR);
		}

		// Don't overflow here either.
		if (result > UINT64_MAX / multiplier)
			goto error;

		result *= multiplier;
	}

	if (result < min || result > max)
		goto error;

	return result;

error:
	errmsg(V_ERROR, _("Value of the option `%s' must be in the range "
				"[%llu, %llu]"), name,
				(unsigned long long)(min),
				(unsigned long long)(max));
	my_exit(ERROR);
}


/// \brief      Gets filename part from pathname+filename
///
/// \return     Pointer in the filename where the actual filename starts.
///             If the last character is a slash, NULL is returned.
///
extern const char *
str_filename(const char *name)
{
	const char *base = strrchr(name, '/');

	if (base == NULL) {
		base = name;
	} else if (*++base == '\0') {
		base = NULL;
		errmsg(V_ERROR, _("%s: Invalid filename"), name);
	}

	return base;
}


/*
/// \brief      Simple quoting to get rid of ASCII control characters
///
/// This is not so cool and locale-dependent, but should be good enough
/// At least we don't print any control characters on the terminal.
///
extern char *
str_quote(const char *str)
{
	size_t dest_len = 0;
	bool has_ctrl = false;

	while (str[dest_len] != '\0')
		if (*(unsigned char *)(str + dest_len++) < 0x20)
			has_ctrl = true;

	char *dest = malloc(dest_len + 1);
	if (dest != NULL) {
		if (has_ctrl) {
			for (size_t i = 0; i < dest_len; ++i)
				if (*(unsigned char *)(str + i) < 0x20)
					dest[i] = '?';
				else
					dest[i] = str[i];

			dest[dest_len] = '\0';

		} else {
			// Usually there are no control characters,
			// so we can optimize.
			memcpy(dest, str, dest_len + 1);
		}
	}

	return dest;
}
*/


extern bool
is_empty_filename(const char *filename)
{
	if (filename[0] == '\0') {
		errmsg(V_WARNING, _("Empty filename, skipping"));
		return true;
	}

	return false;
}
