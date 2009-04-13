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
#include "physmem.h"
#include "cpucores.h"


/// Maximum number of free *coder* threads. This can be set with
/// the --threads=NUM command line option.
static uint32_t threads_max;


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
hardware_threadlimit_init(void)
{
	threads_max = cpucores();
	if (threads_max == 0)
		threads_max = 1;

	return;
}


extern void
hardware_threadlimit_set(uint32_t threadlimit)
{
	threads_max = threadlimit;
	return;
}


extern uint32_t
hardware_threadlimit_get(void)
{
	return threads_max;
}


static void
hardware_memlimit_init(void)
{
	uint64_t mem = physmem();

	// If we cannot determine the amount of RAM, assume 32 MiB. Maybe
	// even that is too much on some systems. But on most systems it's
	// far too little, and can be annoying.
	if (mem == 0)
		mem = UINT64_C(32) * 1024 * 1024;

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
	hardware_threadlimit_init();
	return;
}
