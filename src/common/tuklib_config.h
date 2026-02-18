// SPDX-License-Identifier: 0BSD

// If config.h isn't available, assume that the headers required by
// tuklib_common.h are available. This is required by crc32_tablegen.c.
#ifdef HAVE_CONFIG_H
#	include "sysdefs.h"
#else
#	include <stddef.h>
#	include <stdbool.h>
#	include <inttypes.h>
#	include <limits.h>
#endif

// Omit unneeded code when NLS is disabled.
#ifndef ENABLE_NLS
#	define TUKLIB_MBSTR_WRAP_ENABLE_RTL 0
#endif
