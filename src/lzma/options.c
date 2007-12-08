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

		if (value == NULL || value[0] == '\0') {
			errmsg(V_ERROR, _("%s: Options must be `name=value' "
					"pairs separated with commas"),
					str);
			my_exit(ERROR);
		}

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

				if (opts[i].map[j].name == NULL) {
					errmsg(V_ERROR, _("%s: Invalid option "
							"value"), value);
					my_exit(ERROR);
				}

				set(filter_options, i, j);
			}

			found = true;
			break;
		}

		if (!found) {
			errmsg(V_ERROR, _("%s: Invalid option name"), name);
			my_exit(ERROR);
		}

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
parse_options_subblock(const char *str)
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
	OPT_DISTANCE,
};


static void
set_delta(void *options, uint32_t key, uint64_t value)
{
	lzma_options_delta *opt = options;
	switch (key) {
	case OPT_DISTANCE:
		opt->distance = value;
		break;
	}
}


extern lzma_options_delta *
parse_options_delta(const char *str)
{
	static const option_map opts[] = {
		{ "distance", NULL,  LZMA_DELTA_DISTANCE_MIN,
		                     LZMA_DELTA_DISTANCE_MAX },
		{ NULL,       NULL,  0, 0 }
	};

	lzma_options_delta *options = xmalloc(sizeof(lzma_options_subblock));
	*options = (lzma_options_delta){
		// It's hard to give a useful default for this.
		.distance = LZMA_DELTA_DISTANCE_MIN,
	};

	parse_options(str, opts, &set_delta, options);

	return options;
}


//////////
// LZMA //
//////////

enum {
	OPT_DICT,
	OPT_LC,
	OPT_LP,
	OPT_PB,
	OPT_MODE,
	OPT_FB,
	OPT_MF,
	OPT_MC
};


static void
set_lzma(void *options, uint32_t key, uint64_t value)
{
	lzma_options_lzma *opt = options;

	switch (key) {
	case OPT_DICT:
		opt->dictionary_size = value;
		break;

	case OPT_LC:
		opt->literal_context_bits = value;
		break;

	case OPT_LP:
		opt->literal_pos_bits = value;
		break;

	case OPT_PB:
		opt->pos_bits = value;
		break;

	case OPT_MODE:
		opt->mode = value;
		break;

	case OPT_FB:
		opt->fast_bytes = value;
		break;

	case OPT_MF:
		opt->match_finder = value;
		break;

	case OPT_MC:
		opt->match_finder_cycles = value;
		break;
	}
}


extern lzma_options_lzma *
parse_options_lzma(const char *str)
{
	static const name_id_map modes[] = {
		{ "fast", LZMA_MODE_FAST },
		{ "best", LZMA_MODE_BEST },
		{ NULL,   0 }
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
		{ "dict", NULL,   LZMA_DICTIONARY_SIZE_MIN,
				LZMA_DICTIONARY_SIZE_MAX },
		{ "lc",   NULL,   LZMA_LITERAL_CONTEXT_BITS_MIN,
				  LZMA_LITERAL_CONTEXT_BITS_MAX },
		{ "lp",   NULL,   LZMA_LITERAL_POS_BITS_MIN,
				  LZMA_LITERAL_POS_BITS_MAX },
		{ "pb",   NULL,   LZMA_POS_BITS_MIN, LZMA_POS_BITS_MAX },
		{ "mode", modes,  0, 0 },
		{ "fb",   NULL,   LZMA_FAST_BYTES_MIN, LZMA_FAST_BYTES_MAX },
		{ "mf",   mfs,    0, 0 },
		{ "mc",   NULL,   0, UINT32_MAX },
		{ NULL,   NULL,   0, 0 }
	};

	lzma_options_lzma *options = xmalloc(sizeof(lzma_options_lzma));
	*options = (lzma_options_lzma){
		.dictionary_size = LZMA_DICTIONARY_SIZE_DEFAULT,
		.literal_context_bits = LZMA_LITERAL_CONTEXT_BITS_DEFAULT,
		.literal_pos_bits = LZMA_LITERAL_POS_BITS_DEFAULT,
		.pos_bits = LZMA_POS_BITS_DEFAULT,
		.mode = LZMA_MODE_BEST,
		.fast_bytes = LZMA_FAST_BYTES_DEFAULT,
		.match_finder = LZMA_MF_BT4,
		.match_finder_cycles = 0,
	};

	parse_options(str, opts, &set_lzma, options);

	return options;
}
