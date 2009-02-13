///////////////////////////////////////////////////////////////////////////////
//
/// \file       physmem.h
/// \brief      Get the amount of physical memory
//
//  This code has been put into the public domain.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PHYSMEM_H
#define PHYSMEM_H

#if defined(HAVE_PHYSMEM_SYSCTL) || defined(HAVE_NCPU_SYSCTL)
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#	ifdef HAVE_SYS_SYSCTL_H
#		include <sys/sysctl.h>
#	endif
#endif

#if defined(HAVE_PHYSMEM_SYSCONF) || defined(HAVE_NCPU_SYSCONF)
#	include <unistd.h>
#endif

#ifdef _WIN32
#	ifndef _WIN32_WINNT
#		define _WIN32_WINNT 0x0500
#	endif
#	include <windows.h>
#endif

#ifdef __DJGPP__
#	include <dpmi.h>
#endif


/// \brief      Get the amount of physical memory in bytes
///
/// \return     Amount of physical memory in bytes. On error, zero is
///             returned.
static inline uint64_t
physmem(void)
{
	uint64_t ret = 0;

#if defined(HAVE_PHYSMEM_SYSCONF)
	const long pagesize = sysconf(_SC_PAGESIZE);
	const long pages = sysconf(_SC_PHYS_PAGES);
	if (pagesize != -1 || pages != -1)
		// According to docs, pagesize * pages can overflow.
		// Simple case is 32-bit box with 4 GiB or more RAM,
		// which may report exactly 4 GiB of RAM, and "long"
		// being 32-bit will overflow. Casting to uint64_t
		// hopefully avoids overflows in the near future.
		ret = (uint64_t)(pagesize) * (uint64_t)(pages);

#elif defined(HAVE_PHYSMEM_SYSCTL)
	int name[2] = { CTL_HW, HW_PHYSMEM };
	union {
		unsigned long ul;
		unsigned int ui;
	} mem;
	size_t mem_ptr_size = sizeof(mem.ul);
	if (!sysctl(name, 2, &mem.ul, &mem_ptr_size, NULL, NULL)) {
		// Some systems use unsigned int as the "return value".
		// This makes a difference on 64-bit boxes.
		if (mem_ptr_size == sizeof(mem.ul))
			ret = mem.ul;
		else if (mem_ptr_size == sizeof(mem.ui))
			ret = mem.ui;
	}

#elif defined(_WIN32)
	MEMORYSTATUSEX meminfo;
	meminfo.dwLength = sizeof(meminfo);
	if (GlobalMemoryStatusEx(&meminfo))
		ret = meminfo.ullTotalPhys;

#elif defined(__DJGPP__)
	__dpmi_free_mem_info meminfo;
	if (__dpmi_get_free_memory_information(&meminfo) == 0
			&& meminfo.total_number_of_physical_pages
				!= (unsigned long)(-1))
		ret = (uint64_t)(meminfo.total_number_of_physical_pages)
				* 4096;
#endif

	return ret;
}

#endif
