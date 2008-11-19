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


/// Memory usage limit for encoding
static uint64_t memlimit_encoder;

/// Memory usage limit for decoding
static uint64_t memlimit_decoder;

/// Memory usage limit given on the command line or environment variable.
/// Zero indicates the default (memlimit_encoder or memlimit_decoder).
static uint64_t memlimit_custom = 0;


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


static void
hardware_memlimit_init(void)
{
	uint64_t mem = physmem();

	// If we cannot determine the amount of RAM, assume 32 MiB. Maybe
	// even that is too much on some systems. But on most systems it's
	// far too little, and can be annoying.
	if (mem == 0)
		mem = UINT64_C(16) * 1024 * 1024;

	// Use at maximum of 90 % of RAM when encoding and 33 % when decoding.
	memlimit_encoder = mem - mem / 10;
	memlimit_decoder = mem / 3;

	return;
}


extern void
hardware_memlimit_set(uint64_t memlimit)
{
	memlimit_custom = memlimit;
	return;
}


extern uint64_t
hardware_memlimit_encoder(void)
{
	return memlimit_custom != 0 ? memlimit_custom : memlimit_encoder;
}


extern uint64_t
hardware_memlimit_decoder(void)
{
	return memlimit_custom != 0 ? memlimit_custom : memlimit_decoder;
}


extern void
hardware_init(void)
{
	hardware_memlimit_init();
	hardware_cores();
	return;
}
