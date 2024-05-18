# SPDX-License-Identifier: 0BSD

# [X] tuklib_common.m4
# [X] tuklib_cpucores.m4
# [X] tuklib_integer.m4
# [X] tuklib_mbstr.m4
# [X] tuklib_physmem.m4
# [X] tuklib_progname.m4

# tuklib_common.m4

## requires c99, host, AC_USE_SYSTEM_EXTENSIONS
## nothing to do, here (already in toplevel meson.build)

# tuklib_cpucores.m4

have_sys_param_h = cc.has_header('sys/param.h')

cpu_special_src = '''
#if defined(_WIN32) || defined(__CYGWIN__)
int main(void) { return 0; }
#else
compile error
#endif
'''

cpu_sched_getaffinity_src = '''
#include <sched.h>
int
main(void)
{
	cpu_set_t cpu_mask;
	sched_getaffinity(0, sizeof(cpu_mask), &cpu_mask);
	return CPU_COUNT(&cpu_mask);
}
'''

cpu_cpuset_src = '''
#include <sys/param.h>
#include <sys/cpuset.h>

int
main(void)
{
	cpuset_t set;
	cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1,
			sizeof(set), &set);
	return 0;
}
'''

cpu_sysctl_src = '''
#ifdef __QNX__
compile error
#endif
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main(void)
{
#ifdef HW_NCPUONLINE
	/* This is preferred on OpenBSD, see tuklib_cpucores.c. */
	int name[2] = { CTL_HW, HW_NCPUONLINE };
#else
	int name[2] = { CTL_HW, HW_NCPU };
#endif
	int cpus;
	size_t cpus_size = sizeof(cpus);
	sysctl(name, 2, &cpus, &cpus_size, NULL, 0);
	return 0;
}
'''

cpu_sysconf_src = '''
#include <unistd.h>
int
main(void)
{
	long i;
#ifdef _SC_NPROCESSORS_ONLN
	/* Many systems using sysconf() */
	i = sysconf(_SC_NPROCESSORS_ONLN);
#else
	/* IRIX */
	i = sysconf(_SC_NPROC_ONLN);
#endif
	return 0;
}
'''

cpu_pstat_getdynamic_src = '''
#include <sys/param.h>
#include <sys/pstat.h>

int
main(void)
{
	struct pst_dynamic pst;
	pstat_getdynamic(&pst, sizeof(pst), 1, 0);
	(void)pst.psd_proc_cnt;
	return 0;
}
'''

if cc.compiles(cpu_special_src, name : 'number of cpu cores on Windows or Cygwin')
# nothing to do, here
elif cc.compiles(cpu_sched_getaffinity_src, name : 'number of cpu cores with sched_getaffinity()')
    config_h.set('TUKLIB_CPUCORES_SCHED_GETAFFINITY', true)
elif cc.compiles(cpu_cpuset_src, name : 'number of cpu cores with cpuset()')
    config_h.set('TUKLIB_CPUCORES_CPUSET', true)
elif cc.compiles(cpu_sysctl_src, args : have_sys_param_h ? '-DHAVE_SYS_PARAM_H' : '', name : 'number of cpu cores with sysctl()')
    config_h.set('TUKLIB_CPUCORES_SYSCTL', true)
elif cc.compiles(cpu_sysconf_src, name : 'number of cpu cores with sysconf()')
    config_h.set('TUKLIB_CPUCORES_SYSCONF', true)
elif cc.compiles(cpu_pstat_getdynamic_src, name : 'number of cpu cores with pstat_getdynamic()')
    config_h.set('TUKLIB_CPUCORES_PSTAT_GETDYNAMIC', true)
endif

# tuklib_integer.m4

bswap_src = '''
int main()
{
	__builtin_bswap16(1);
	__builtin_bswap32(1);
	__builtin_bswap64(1);
	return 0;
}
'''
res = cc.links(bswap_src, name: '__builtin_bswap16/32/64')
if res
    config_h.set('HAVE___BUILTIN_BSWAPXX', true,
        description: 'Define to 1 if the GNU C extensions __builtin_bswap16/32/64 are supported.'
        )
else
    have_byteswap_h = false
    have_sys_endian_h = false
    have_sys_byteorder_h = false
    bswap_headers = [
        'byteswap.h',
        'sys/endian.h',
        'sys/byteorder.h'
    ]
    foreach h : bswap_headers
        if cc.has_header(h)
            config_h.set('HAVE_' + h.underscorify().to_upper(), true)
            set_variable(f'have_' + h.underscorify(), true)
        endif
    endforeach

    if have_byteswap_h
        bswap_fcts = [
            'bswap_16',
            'bswap_32',
            'bswap_64'
        ]
        foreach f : bswap_fcts
            bswap_src = '''
#include <byteswap.h>
int main()
{
	@0@(42);
	return 0;
}
'''
            config_h.set('HAVE_' + f.to_upper(),
                cc.links(bswap_src.format(f), name: f),
                description: 'Define to 1 if ' + f + '() is available.'
            )
        endforeach
    endif
endif

fast_unaligned_access = false
if get_option('unaligned-access').auto()
    unaligned_access_cpus = [
        'ppc',
        'ppc64',
        'x86',
        'x86_64',
    ]
    if host_machine.cpu_family() in unaligned_access_cpus
        config_h.set('TUKLIB_FAST_UNALIGNED_ACCESS', true,
            description: 'Define to 1 if the system supports fast unaligned access to 16-bit, 32-bit, and 64-bit integers.'
        )
        fast_unaligned_access = true
    endif

    if not fast_unaligned_access
        unaligned_access_cpus = [
            'aarch64',
            'arm',
            'riscv32',
            'riscv64',
        ]
        if host_machine.cpu_family() in unaligned_access_cpus
            ua_src = '''
#if !defined(__ARM_FEATURE_UNALIGNED) \
		&& !defined(__riscv_misaligned_fast) \
		&& !defined(_MSC_VER)
compile error
#endif
int main(void) { return 0; }
'''
            if cc.compiles(ua_src, name: 'fast unaligned access for ' + host_machine.cpu_family())
                config_h.set('TUKLIB_FAST_UNALIGNED_ACCESS', true,
                description: 'Define to 1 if the system supports fast unaligned access to 16-bit, 32-bit, and 64-bit integers.'
                )
                fast_unaligned_access = true
            endif
        endif
    endif
elif get_option('unaligned-access').enabled()
    config_h.set0('TUKLIB_FAST_UNALIGNED_ACCESS', true,
        description: 'Define to 1 if the system supports fast unaligned access to 16-bit, 32-bit, and 64-bit integers.'
    )
    fast_unaligned_access = true
endif

if get_option('unsafe-type-punning')
    config_h.set('HAVE___BUILTIN_ASSUME_ALIGNED', true,
        description: 'Define to 1 if the GNU C extension __builtin_assume_aligned is supported.'
                    )
endif

# tuklib_mbstr.m4

### FIXME : mbrtowc is supported in C99 spec and on Windows
if cc.has_header('wchar.h')

    if cc.has_function('wcwidth',
        args : '-D_XOPEN_SOURCE',
        prefix : '#include <wchar.h>'
        )
        config_h.set('HAVE_WCWIDTH', true,
            description: 'Define to 1 if the wcwidth() function is available.'
        )
    endif
endif

# tuklib_physmem.m4


physmem_special_src = '''
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__OS2__) \
		|| defined(__DJGPP__) || defined(__VMS) \
		|| defined(AMIGA) || defined(__AROS__) || defined(__QNX__)
int main(void) { return 0; }
#else
compile error
#endif
'''

physmem_aix_src = '''
#include <sys/systemcfg.h>

int
main(void)
{
	(void)_system_configuration.physmem;
	return 0;
}
'''

physmem_sysconf_src = '''
#include <unistd.h>
int
main(void)
{
	long i;
	i = sysconf(_SC_PAGESIZE);
	i = sysconf(_SC_PHYS_PAGES);
	return 0;
}
'''

physmem_sysctl_src = '''
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main(void)
{
	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	sysctl(name, 2, &mem, &mem_ptr_size, NULL, 0);
	return 0;
}
'''

physmem_getsysinfo_src = '''
#include <sys/sysinfo.h>
#include <machine/hal_sysinfo.h>

int
main(void)
{
	int memkb;
	int start = 0;
	getsysinfo(GSI_PHYSMEM, (caddr_t)&memkb, sizeof(memkb), &start);
	return 0;
}
'''

physmem_pstat_getstatic_src = '''
#include <sys/param.h>
#include <sys/pstat.h>

int
main(void)
{
	struct pst_static pst;
	pstat_getstatic(&pst, sizeof(pst), 1, 0);
	(void)pst.physical_memory;
	(void)pst.page_size;
	return 0;
}
'''

physmem_getinvent_r_src = '''
#include <invent.h>
int
main(void)
{
	inv_state_t *st = NULL;
	setinvent_r(&st);
	getinvent_r(st);
	endinvent_r(st);
	return 0;
}
'''

physmem_sysinfo_src = '''
#include <sys/sysinfo.h>
int
main(void)
{
	struct sysinfo si;
	sysinfo(&si);
	return 0;
}
'''

if cc.compiles(physmem_special_src, name : 'physical memory on some special OSes')
# nothing to do, here
elif cc.compiles(physmem_aix_src, name : 'physical memory on AIX')
    config_h.set('TUKLIB_PHYSMEM_AIX', true)
elif cc.compiles(physmem_sysconf_src, name : 'physical memory with sysconf()')
    config_h.set('TUKLIB_PHYSMEM_SYSCONF', true)
elif cc.compiles(physmem_sysctl_src, args : have_sys_param_h ? '-DHAVE_SYS_PARAM_H' : '', name : 'physical memory with sysctl()')
    config_h.set('TUKLIB_PHYSMEM_SYSCTL', true)
elif cc.compiles(physmem_getsysinfo_src, name : 'physical memory with getsysinfo()')
    config_h.set('TUKLIB_PHYSMEM_GETSYSINFO', true)
elif cc.compiles(physmem_pstat_getstatic_src, name : 'physical memory with pstat_getstatic()')
    config_h.set('TUKLIB_PHYSMEM_PSTAT_GETSTATIC', true)
elif cc.compiles(physmem_getinvent_r_src, name : 'physical memory with getinvent_r()')
    config_h.set('TUKLIB_PHYSMEM_GETINVENT_R', true)
elif cc.compiles(physmem_sysinfo_src, name : 'physical memory with sysinfo()')
    config_h.set('TUKLIB_PHYSMEM_SYSINFO', true)
endif

# tuklib_progname.m4

if cc.has_function('program_invocation_name',
    prefix : '#include <errno.h>'
    )
    config_h.set('HAVE_PROGRAM_INVOCATION_NAME', true,
        description: 'Define to 1 if the program_invocation_name() function is available.'
    )
endif
