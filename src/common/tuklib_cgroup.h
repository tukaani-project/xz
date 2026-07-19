// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       tuklib_cgroup.h
/// \brief      Read Linux cgroup v2 limits
///
/// These are only available on Linux.
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TUKLIB_CGROUP_H
#define TUKLIB_CGROUP_H

#include "tuklib_common.h"
#ifdef __linux__
TUKLIB_DECLS_BEGIN

#define tuklib_cgroup_memory_max TUKLIB_SYMBOL(tuklib_cgroup_memory_max)
extern unsigned long long tuklib_cgroup_memory_max(void);
///<
/// \brief      Linux only: Get the effective cgroup v2 memory.max
///
/// \return     The effective cgroup v2 memory.max as bytes, or ULLONG_MAX
///             if there is no limit or the lookup fails for any reason.

#define tuklib_cgroup_cpu_max TUKLIB_SYMBOL(tuklib_cgroup_cpu_max)
extern unsigned long long tuklib_cgroup_cpu_max(void);
///<
/// \brief      Linux only: Get the effective cgroup v2 cpu.max
///
/// \return     The effective cgroup v2 cpu.max as a number of cores that can
///             be fully utilized without being throttled or at least 1 (in
///             case cpu.max is less than 100 % of one core). ULLONG_MAX is
///             returned if there is no limit or the lookup fails.

TUKLIB_DECLS_END
#endif
#endif
