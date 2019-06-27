///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.c
/// \brief      Detection of available hardware resources
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"


/// Maximum number of worker threads. This can be set with
/// the --threads=NUM command line option.
static uint32_t threads_max = 1;

/// Memory usage limit for compression
static uint64_t memlimit_compress;

/// Memory usage limit for decompression
static uint64_t memlimit_decompress;

/// Total amount of physical RAM
static uint64_t total_ram;


extern void
hardware_threads_set(uint32_t n)
{
	if (n == 0) {
		// Automatic number of threads was requested.
		// If threading support was enabled at build time,
		// use the number of available CPU cores. Otherwise
		// use one thread since disabling threading support
		// omits lzma_cputhreads() from liblzma.
#ifdef MYTHREAD_ENABLED
		threads_max = lzma_cputhreads();
		if (threads_max == 0)
			threads_max = 1;
#else
		threads_max = 1;
#endif
	} else {
		threads_max = n;
	}

	return;
}


extern uint32_t
hardware_threads_get(void)
{
	return threads_max;
}


extern void
hardware_memlimit_set(uint64_t new_memlimit,
		bool set_compress, bool set_decompress, bool is_percentage)
{
	if (is_percentage) {
		assert(new_memlimit > 0);
		assert(new_memlimit <= 100);
		new_memlimit = (uint32_t)new_memlimit * total_ram / 100;
	}

	if (set_compress)
		memlimit_compress = new_memlimit;

	if (set_decompress)
		memlimit_decompress = new_memlimit;

	return;
}


extern uint64_t
hardware_memlimit_get(enum operation_mode mode)
{
	// Zero is a special value that indicates the default. Currently
	// the default simply disables the limit. Once there is threading
	// support, this might be a little more complex, because there will
	// probably be a special case where a user asks for "optimal" number
	// of threads instead of a specific number (this might even become
	// the default mode). Each thread may use a significant amount of
	// memory. When there are no memory usage limits set, we need some
	// default soft limit for calculating the "optimal" number of
	// threads.
	const uint64_t memlimit = mode == MODE_COMPRESS
			? memlimit_compress : memlimit_decompress;
	return memlimit != 0 ? memlimit : UINT64_MAX;
}


/// Helper for hardware_memlimit_show() to print one human-readable info line.
static void
memlimit_show(const char *str, size_t str_columns, uint64_t value)
{
	// Calculate the field width so that str will be padded to take
	// str_columns on the terminal.
	//
	// NOTE: If the string is invalid, this will be -1. Using -1 as
	// the field width is fine here so it's not handled specially.
	const int fw = tuklib_mbstr_fw(str, (int)(str_columns));

	// The memory usage limit is considered to be disabled if value
	// is 0 or UINT64_MAX. This might get a bit more complex once there
	// is threading support. See the comment in hardware_memlimit_get().
	if (value == 0 || value == UINT64_MAX)
		printf("%-*s  %s\n", fw, str, _("Disabled"));
	else
		printf("%-*s  %s MiB (%s B)\n", fw, str,
				uint64_to_str(round_up_to_mib(value), 0),
				uint64_to_str(value, 1));

	return;
}


extern void
hardware_memlimit_show(void)
{
	if (opt_robot) {
		printf("%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n", total_ram,
				memlimit_compress, memlimit_decompress);
	} else {
		const char *msgs[] = {
			_("Amount of physical memory (RAM):"),
			_("Memory usage limit for compression:"),
			_("Memory usage limit for decompression:"),
		};

		size_t width_max = 1;
		for (unsigned i = 0; i < ARRAY_SIZE(msgs); ++i) {
			size_t w = tuklib_mbstr_width(msgs[i], NULL);

			// When debugging, catch invalid strings with
			// an assertion. Otherwise fallback to 1 so
			// that the columns just won't be aligned.
			assert(w != (size_t)-1);
			if (w == (size_t)-1)
				w = 1;

			if (width_max < w)
				width_max = w;
		}

		memlimit_show(msgs[0], width_max, total_ram);
		memlimit_show(msgs[1], width_max, memlimit_compress);
		memlimit_show(msgs[2], width_max, memlimit_decompress);
	}

	tuklib_exit(E_SUCCESS, E_ERROR, message_verbosity_get() != V_SILENT);
}


extern void
hardware_init(void)
{
	// Get the amount of RAM. If we cannot determine it,
	// use the assumption defined by the configure script.
	total_ram = lzma_physmem();
	if (total_ram == 0)
		total_ram = (uint64_t)(ASSUME_RAM) * 1024 * 1024;

	// Set the defaults.
	hardware_memlimit_set(0, true, true, false);
	return;
}
