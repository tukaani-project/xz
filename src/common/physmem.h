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
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	if (!sysctl(name, 2, &mem, &mem_ptr_size, NULL, NULL)) {
		// Some systems use unsigned int as the "return value".
		// This makes a difference on 64-bit boxes.
		if (mem_ptr_size != sizeof(mem)) {
			if (mem_ptr_size == sizeof(unsigned int))
				ret = *(unsigned int *)(&mem);
		} else {
			ret = mem;
		}
	}
#endif

	return ret;
}

#endif
