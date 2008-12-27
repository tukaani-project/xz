///////////////////////////////////////////////////////////////////////////////
//
/// \file       options.c
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

#include "private.h"


///////////////////
// Generic stuff //
///////////////////

typedef struct {
	const char *name;
	uint64_t id;
} name_id_map;


typedef struct {
	const char *name;
	const name_id_map *map;
	uint64_t min;
	uint64_t max;
} option_map;


/// Parses option=value pairs that are separated with colons, semicolons,
/// or commas: opt=val:opt=val;opt=val,opt=val
///
/// Each option is a string, that is converted to an integer using the
/// index where the option string is in the array.
///
/// Value can be either a number with minimum and maximum value limit, or
/// a string-id map mapping a list of possible string values to integers.
///
/// When parsing both option and value succeed, a filter-specific function
/// is called, which should update the given value to filter-specific
/// options structure.
///
/// \param      str     String containing the options from the command line
/// \param      opts    Filter-specific option map
/// \param      set     Filter-specific function to update filter_options
/// \param      filter_options  Pointer to filter-specific options structure
///
/// \return     Returns only if no errors occur.
///
static void
parse_options(const char *str, const option_map *opts,
		void (*set)(void *filter_options,
			uint32_t key, uint64_t value),
		void *filter_options)
{
	if (str == NULL || str[0] == '\0')
		return;

	char *s = xstrdup(str);
	char *name = s;

	while (true) {
		char *split = strchr(name, ',');
		if (split != NULL)
			*split = '\0';

		char *value = strchr(name, '=');
		if (value != NULL)
			*value++ = '\0';

		if (value == NULL || value[0] == '\0')
			message_fatal(_("%s: Options must be `name=value' "
					"pairs separated with commas"), str);

		// Look for the option name from the option map.
		bool found = false;
		for (size_t i = 0; opts[i].name != NULL; ++i) {
			if (strcmp(name, opts[i].name) != 0)
				continue;

			if (opts[i].map == NULL) {
				// value is an integer.
				const uint64_t v = str_to_uint64(name, value,
						opts[i].min, opts[i].max);
				set(filter_options, i, v);
			} else {
				// value is a string which we should map
				// to an integer.
				size_t j;
				for (j = 0; opts[i].map[j].name != NULL; ++j) {
					if (strcmp(opts[i].map[j].name, value)
							== 0)
						break;
				}

				if (opts[i].map[j].name == NULL)
					message_fatal(_("%s: Invalid option "
							"value"), value);

				set(filter_options, i, opts[i].map[j].id);
			}

			found = true;
			break;
		}

		if (!found)
			message_fatal(_("%s: Invalid option name"), name);

		if (split == NULL)
			break;

		name = split + 1;
	}

	free(s);
	return;
}


//////////////
// Subblock //
//////////////

enum {
	OPT_SIZE,
	OPT_RLE,
	OPT_ALIGN,
};


static void
set_subblock(void *options, uint32_t key, uint64_t value)
{
	lzma_options_subblock *opt = options;

	switch (key) {
	case OPT_SIZE:
		opt->subblock_data_size = value;
		break;

	case OPT_RLE:
		opt->rle = value;
		break;

	case OPT_ALIGN:
		opt->alignment = value;
		break;
	}
}


extern lzma_options_subblock *
options_subblock(const char *str)
{
	static const option_map opts[] = {
		{ "size", NULL,   LZMA_SUBBLOCK_DATA_SIZE_MIN,
		                  LZMA_SUBBLOCK_DATA_SIZE_MAX },
		{ "rle",  NULL,   LZMA_SUBBLOCK_RLE_OFF,
		                  LZMA_SUBBLOCK_RLE_MAX },
		{ "align",NULL,   LZMA_SUBBLOCK_ALIGNMENT_MIN,
		                  LZMA_SUBBLOCK_ALIGNMENT_MAX },
		{ NULL,   NULL,   0, 0 }
	};

	lzma_options_subblock *options
			= xmalloc(sizeof(lzma_options_subblock));
	*options = (lzma_options_subblock){
		.allow_subfilters = false,
		.alignment = LZMA_SUBBLOCK_ALIGNMENT_DEFAULT,
		.subblock_data_size = LZMA_SUBBLOCK_DATA_SIZE_DEFAULT,
		.rle = LZMA_SUBBLOCK_RLE_OFF,
	};

	parse_options(str, opts, &set_subblock, options);

	return options;
}


///////////
// Delta //
///////////

enum {
	OPT_DIST,
};


static void
set_delta(void *options, uint32_t key, uint64_t value)
{
	lzma_options_delta *opt = options;
	switch (key) {
	case OPT_DIST:
		opt->dist = value;
		break;
	}
}


extern lzma_options_delta *
options_delta(const char *str)
{
	static const option_map opts[] = {
		{ "dist",     NULL,  LZMA_DELTA_DIST_MIN,
		                     LZMA_DELTA_DIST_MAX },
		{ NULL,       NULL,  0, 0 }
	};

	lzma_options_delta *options = xmalloc(sizeof(lzma_options_delta));
	*options = (lzma_options_delta){
		// It's hard to give a useful default for this.
		.type = LZMA_DELTA_TYPE_BYTE,
		.dist = LZMA_DELTA_DIST_MIN,
	};

	parse_options(str, opts, &set_delta, options);

	return options;
}


//////////
// LZMA //
//////////

enum {
	OPT_PRESET,
	OPT_DICT,
	OPT_LC,
	OPT_LP,
	OPT_PB,
	OPT_MODE,
	OPT_NICE,
	OPT_MF,
	OPT_DEPTH,
};


static void
set_lzma(void *options, uint32_t key, uint64_t value)
{
	lzma_options_lzma *opt = options;

	switch (key) {
	case OPT_PRESET:
		if (lzma_lzma_preset(options, (uint32_t)(value)))
			message_fatal("LZMA1/LZMA2 preset %u is not supported",
					(unsigned int)(value));
		break;

	case OPT_DICT:
		opt->dict_size = value;
		break;

	case OPT_LC:
		opt->lc = value;
		break;

	case OPT_LP:
		opt->lp = value;
		break;

	case OPT_PB:
		opt->pb = value;
		break;

	case OPT_MODE:
		opt->mode = value;
		break;

	case OPT_NICE:
		opt->nice_len = value;
		break;

	case OPT_MF:
		opt->mf = value;
		break;

	case OPT_DEPTH:
		opt->depth = value;
		break;
	}
}


extern lzma_options_lzma *
options_lzma(const char *str)
{
	static const name_id_map modes[] = {
		{ "fast",   LZMA_MODE_FAST },
		{ "normal", LZMA_MODE_NORMAL },
		{ NULL,     0 }
	};

	static const name_id_map mfs[] = {
		{ "hc3", LZMA_MF_HC3 },
		{ "hc4", LZMA_MF_HC4 },
		{ "bt2", LZMA_MF_BT2 },
		{ "bt3", LZMA_MF_BT3 },
		{ "bt4", LZMA_MF_BT4 },
		{ NULL,  0 }
	};

	static const option_map opts[] = {
		{ "preset", NULL,   0, 9 },
		{ "dict",   NULL,   LZMA_DICT_SIZE_MIN,
				(UINT32_C(1) << 30) + (UINT32_C(1) << 29) },
		{ "lc",     NULL,   LZMA_LCLP_MIN, LZMA_LCLP_MAX },
		{ "lp",     NULL,   LZMA_LCLP_MIN, LZMA_LCLP_MAX },
		{ "pb",     NULL,   LZMA_PB_MIN, LZMA_PB_MAX },
		{ "mode",   modes,  0, 0 },
		{ "nice",   NULL,   2, 273 },
		{ "mf",     mfs,    0, 0 },
		{ "depth",  NULL,   0, UINT32_MAX },
		{ NULL,     NULL,   0, 0 }
	};

	// TODO There should be a way to take some preset as the base for
	// custom settings.
	lzma_options_lzma *options = xmalloc(sizeof(lzma_options_lzma));
	*options = (lzma_options_lzma){
		.dict_size = LZMA_DICT_SIZE_DEFAULT,
		.preset_dict =  NULL,
		.preset_dict_size = 0,
		.lc = LZMA_LC_DEFAULT,
		.lp = LZMA_LP_DEFAULT,
		.pb = LZMA_PB_DEFAULT,
		.persistent = false,
		.mode = LZMA_MODE_NORMAL,
		.nice_len = 64,
		.mf = LZMA_MF_BT4,
		.depth = 0,
	};

	parse_options(str, opts, &set_lzma, options);

	if (options->lc + options->lp > LZMA_LCLP_MAX)
		message_fatal(_("The sum of lc and lp must be at "
				"maximum of 4"));

	const uint32_t nice_len_min = options->mf & 0x0F;
	if (options->nice_len < nice_len_min)
		message_fatal(_("The selected match finder requires at "
				"least nice=%" PRIu32), nice_len_min);

	return options;
}
