// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_physmem.h
/// \brief      Get the amount of physical memory
///
/// This depends on tuklib_cgroup.c.
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TUKLIB_PHYSMEM_H
#define TUKLIB_PHYSMEM_H

#include "tuklib_common.h"
TUKLIB_DECLS_BEGIN

#define tuklib_physmem TUKLIB_SYMBOL(tuklib_physmem)
extern uint64_t tuklib_physmem(void);
///<
/// \brief      Get the amount of physical memory in bytes
///
/// On Linux, if cgroup v2 memory.max is lower than the amount of physical
/// memory, the memory.max value is returned.
///
/// \return     Amount of physical memory in bytes. On error, zero is
///             returned.

TUKLIB_DECLS_END
#endif
