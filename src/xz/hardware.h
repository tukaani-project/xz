///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.h
/// \brief      Detection of available hardware resources
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// Initialize some hardware-specific variables, which are needed by other
/// hardware_* functions.
extern void hardware_init(void);


/// Set custom value for maximum number of coder threads.
extern void hardware_threadlimit_set(uint32_t threadlimit);

/// Get the maximum number of coder threads. Some additional helper threads
/// are allowed on top of this).
extern uint32_t hardware_threadlimit_get(void);


/// Set custom memory usage limit. This is used for both encoding and
/// decoding. Zero indicates resetting the limit back to defaults.
extern void hardware_memlimit_set(uint64_t memlimit);

/// Get the memory usage limit for encoding. By default this is 90 % of RAM.
extern uint64_t hardware_memlimit_encoder(void);


/// Get the memory usage limit for decoding. By default this is 30 % of RAM.
extern uint64_t hardware_memlimit_decoder(void);
