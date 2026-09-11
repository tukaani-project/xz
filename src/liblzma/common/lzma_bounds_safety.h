// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_bounds_safety.h
/// \brief      Portability macros for optional Clang -fbounds-safety
///
/// When LZMA_SUPPORT_FBOUNDS_SAFETY is defined (typically via
/// -DLZMA_SUPPORT_FBOUNDS_SAFETY and a Clang toolchain that implements
/// -fbounds-safety), these macros expand to Clang bounds annotations.
/// Otherwise they expand to nothing so default builds are unchanged.
///
/// Pattern matches the libwebp / libpng inert-macro adoption:
/// annotations are inert unless explicitly enabled.
//
//  Author:     Jeff Bindel
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_BOUNDS_SAFETY_H
#define LZMA_BOUNDS_SAFETY_H

#ifdef LZMA_SUPPORT_FBOUNDS_SAFETY

#	include <ptrcheck.h>
/* Non-ABI-breaking counted-by annotations for struct pointer members.
 * Prefer __counted_by_or_null for pointers that may be NULL while the
 * companion size field is zero (lzma_dict.buf pattern).
 */
#	define LZMA_COUNTED_BY(n) __counted_by(n)
#	define LZMA_COUNTED_BY_OR_NULL(n) __counted_by_or_null(n)

#else /* !LZMA_SUPPORT_FBOUNDS_SAFETY */

#	define LZMA_COUNTED_BY(n)
#	define LZMA_COUNTED_BY_OR_NULL(n)

#endif /* LZMA_SUPPORT_FBOUNDS_SAFETY */

#endif /* LZMA_BOUNDS_SAFETY_H */
