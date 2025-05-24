// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       my_allocator.h
/// \brief      lzma_allocator to use malloc() and free() from app's libc
///
/// In 1980s and early 1990s, some operating systems implemented shared
/// libraries so that not only the code but also the data is shared
/// between processes. If an application allocates memory by calling into
/// a shared library, it also needs to call into the library to free the
/// memory before the application terminates. Otherwise memory is leaked
/// in the shared library.
///
/// The memory allocated by the application itself *is* freed on those
/// OSes when the application terminates. It's only the memory (and other
/// resources) allocated in shared libraries that can be a problem. If
/// a shared library is made to use application's malloc() and free(),
/// the problem is solved as long as the shared library doesn't also
/// allocate some other types of resources. Thus, this kind of leak
/// prevention is incompatible with threads.
///
/// A related issue is that if the shared library allocates memory with
/// malloc() and the resulting pointer is passed to the application, the
/// application cannot free the memory using free(). This is because the
/// shared library and application have their own heaps. This too is
/// solved if the shared library is made to use application's malloc()
/// and free(). (This issue is possible on Windows even in modern times
/// if a DLL uses a different CRT than the application. However, it is
/// reasonable to assume that XZ Utils components all use the same CRT.)
///
/// The allocator in this header is a hack that should only be enabled
/// on those OSes that really need it. liblzma uses not only malloc() and
/// free() but also calloc(). When a custom allocator is used, calloc()
/// is replaced with allocator->alloc() + memset() which is significantly
/// slower in certain situations where most of the allocated memory isn't
/// actually needed (compressing a tiny file with the LZMA2 dictionary
/// size set to a large value).
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MY_ALLOCATOR_H
#define MY_ALLOCATOR_H

#include "sysdefs.h"
#include "lzma.h"


#ifdef __MORPHOS__

static void * LZMA_API_CALL
my_alloc(void *opaque lzma_attribute((__unused__)), size_t nmemb, size_t size)
{
	// liblzma guarantees that this won't overflow.
	return malloc(nmemb * size);
}

static void LZMA_API_CALL
my_free(void *opaque lzma_attribute((__unused__)), void *ptr)
{
	free(ptr);
}

static const lzma_allocator my_allocator = { &my_alloc, &my_free, NULL };

#define MY_ALLOCATOR (&my_allocator)
#define MY_ALLOCATOR_SET(strm) ((strm).allocator = &my_allocator)

#else

// OSes with modern shared library mechanism don't need the allocator hack.
// For example, this isn't needed on Windows 95.
#define MY_ALLOCATOR NULL
#define MY_ALLOCATOR_SET(strm) ((void)0)

#endif

#endif
