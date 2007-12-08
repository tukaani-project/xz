///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.c
/// \brief      Detection of available hardware resources
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
#include "physmem.h"


/// Maximum number of free *coder* threads. This can be set with
/// the --threads=NUM command line option.
size_t opt_threads = 1;


/// Number of bytes of memory to use at maximum (only a rough limit).
/// This can be set with the --memory=NUM command line option.
/// If no better value can be determined, the default is 14 MiB, which
/// should be quite safe even for older systems while still allowing
/// reasonable compression ratio.
size_t opt_memory = 14 * 1024 * 1024;


/// Get the amount of physical memory, and set opt_memory to 1/3 of it.
/// User can then override this with --memory command line option.
static void
hardware_memory(void)
{
	uint64_t mem = physmem();
	if (mem != 0) {
		mem /= 3;

#if UINT64_MAX > SIZE_MAX
		if (mem > SIZE_MAX)
			mem = SIZE_MAX;
#endif

		opt_memory = mem;
	}

	return;
}


/// Get the number of CPU cores, and set opt_threads to default to that value.
/// User can then override this with --threads command line option.
static void
hardware_cores(void)
{
#if defined(HAVE_NUM_PROCESSORS_SYSCONF)
	const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpus > 0)
		opt_threads = (size_t)(cpus);

#elif defined(HAVE_NUM_PROCESSORS_SYSCTL)
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	if (!sysctl(name, &cpus, &cpus_size, NULL, NULL)
			&& cpus_size == sizeof(cpus) && cpus > 0)
		opt_threads = (size_t)(cpus);
#endif

	// Limit opt_threads so that maximum number of threads doesn't exceed.

#if defined(_SC_THREAD_THREADS_MAX)
	const long threads_max = sysconf(_SC_THREAD_THREADS_MAX);
	if (threads_max > 0 && (size_t)(threads_max) < opt_threads)
		opt_threads = (size_t)(threads_max);

#elif defined(PTHREAD_THREADS_MAX)
	if (opt_threads > PTHREAD_THREADS_MAX)
		opt_threads = PTHREAD_THREADS_MAX;
#endif

	return;
}


extern void
hardware_init(void)
{
	hardware_memory();
	hardware_cores();
	return;
}
