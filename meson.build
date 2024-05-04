## SPDX-License-Identifier: 0BSD
## Author: Vincent Torri

project(
    'xz-utils',
    'c',
    version: '5.6.99',
    license: '0BSD',
    license_files: [
        'COPYING',
        'COPYING.0BSD',
        'COPYING.GPLv2',
        'COPYING.GPLv3',
        'COPYING.LGPLv2.1'
    ],
    default_options: [
        'b_ndebug=if-release',
        'c_std=c11',
        'warning_level=3',
    ],
    meson_version: '>= 1.1.0',
)

v_array = meson.project_version().split('.')
v_maj = v_array[0]

# config.h

config_dir = [include_directories('.')]

config_h = configuration_data()
config_h.set_quoted('PACKAGE', 'xz')
config_h.set_quoted('PACKAGE_BUGREPORT', 'xz@tukaani.org')
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_URL', 'https://tukaani.org/xz/')

# host

config_h.set10('WORDS_BIGENDIAN', host_machine.endian() == 'big',
    description: 'Define to 1 if the processor stores words with the most significant byte first.'
)

host_os = host_machine.system()

cygwin = 'cygwin'
windows = 'windows'
linux = 'linux'
sunos = 'sunos'
asm_os = [ 'linux', 'dragonfly', 'freebsd', 'netbsd', 'openbsd', 'windows', 'cygwin' ]
sys_cygwin = cygwin.contains(host_os)
sys_windows = windows.contains(host_os)

COND_W32 = sys_cygwin or sys_windows

# binaries
cc = meson.get_compiler('c')

# try mimic AC_USE_SYSTEM_EXTENSIONS
if host_machine.system() == 'linux'
    add_project_arguments('-D_GNU_SOURCE', language: 'c')
elif host_machine.system() == 'sunos'
    add_project_arguments('-D__EXTENSIONS__', language: 'c')
    add_project_arguments('-D_POSIX_PTHREAD_SEMANTICS', language: 'c')
else
    add_project_arguments('-D_TANDEM_SOURCE', language: 'c')
    add_project_arguments('-D_ALL_SOURCE', language: 'c')
endif

lzma_c_args = []
cflags_try = []

if cc.get_argument_syntax() == 'gcc'
    cflags_try += [
        '-Wvla',
        '-Wformat=2',
        '-Winit-self',
        '-Wmissing-include-dirs',
        '-Wshift-overflow=2',
        '-Wstrict-overflow=3',
        '-Walloc-zero',
        '-Wduplicated-cond',
        '-Wfloat-equal',
        '-Wundef',
        '-Wshadow',
        '-Wpointer-arith',
        '-Wbad-function-cast',
        '-Wwrite-strings',
        '-Wdate-time',
        '-Wsign-conversion',
        '-Wfloat-conversion',
        '-Wlogical-op',
        '-Waggregate-return',
        '-Wstrict-prototypes',
        '-Wold-style-definition',
        '-Wmissing-prototypes',
        '-Wmissing-declarations',
        '-Wredundant-decls',
#
        '-Wc99-compat',
        '-Wc11-extensions',
        '-Wc2x-compat',
        '-Wc2x-extensions',
        '-Wpre-c2x-compat',
        '-Warray-bounds-pointer-arithmetic',
        '-Wassign-enum',
        '-Wconditional-uninitialized',
        '-Wdocumentation',
        '-Wduplicate-enum',
        '-Wempty-translation-unit',
        '-Wflexible-array-extensions',
        '-Wmissing-variable-declarations',
        '-Wnewline-eof',
        '-Wshift-sign-overflow',
        '-Wstring-conversion',
    ]

    lzma_c_args += cc.get_supported_arguments(cflags_try)
endif

config_h.set10('HAVE_VISIBILITY', cc.has_function_attribute('visibility'),
    description: 'Define to 1 if the compiler supports simple visibility declarations.'
)

ctor_fct_src = '''
__attribute__((__constructor__))
static void my_constructor_func(void) { return; }
int main() { return 0; }
'''
if cc.compiles(ctor_fct_src, name : 'constructor attribute for functions')
    config_h.set10('HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR', true,
        description: 'Define to 1 if __attribute__((__constructor__)) is supported for functions.'
    )
endif

cc.has_type('uint32_t', prefix: '#include <stdint.h>')

time_fcts = [
    [ 'futimens',  [ '#include <sys/stat.h>' ] ],
    [ 'futimes',   [ '#include <sys/time.h>' ] ],
    [ 'futimesat', [ '#include <fcntl.h>', '#include <sys/time.h>' ] ],
    [ 'utimes',    [ '#include <sys/time.h>' ] ],
    [ '_futime',   [ '#include <sys/utime.h>' ] ],
    [ 'utime',     [ '#include <sys/types.h>', '#include <utime.h>' ] ],
]
foreach f : time_fcts
    if cc.has_function(f[0], prefix : f[1])
        config_h.set10('HAVE_' + f[0].to_upper(), true,
            description: 'Define to 1 if the ' + f[0] + '() function is available.'
        )
        break
    endif
endforeach

if cc.has_function('posix_advice', prefix : '#include <fcntl.h>')
    config_h.set10('HAVE_POSIX_ADVICE', true,
        description: 'Define to 1 if the posix_advice() function is available.'
    )
endif

### Filters

## encoders
encoder_msg = ''
decoder_msg = ''

supported_filters = [
    'lzma1',
    'lzma2',
    'delta',
    'x86',
    'powerpc',
    'ia64',
    'arm',
    'armthumb',
    'arm64',
    'sparc',
    'riscv',
]

simple_filters = [
    'x86',
    'powerpc',
    'ia64',
    'arm',
    'armthumb',
    'arm64',
    'sparc',
    'riscv',
]

# FIXME foreach i in array; set_variable(f'have_@i@', true) endforeach

foreach f : supported_filters
    if f in get_option('encoders')
        config_h.set10('HAVE_ENCODER_' + f.underscorify().to_upper(), true,
            description: 'Define to 1 if ' + f + ' encoder is enabled.'
        )
        encoder_msg += f + ' '
    endif
    if f in get_option('decoders')
        config_h.set10('HAVE_DECODER_' + f.underscorify().to_upper(), true,
            description: 'Define to 1 if ' + f + ' decoder is enabled.'
        )
        decoder_msg += f + ' '
    endif
endforeach

have_encoder_simple_filters = false
have_decoder_simple_filters = false
foreach f : simple_filters
    if f in get_option('encoders')
        have_encoder_simple_filters = true
    endif
    if f in get_option('decoders')
        have_decoder_simple_filters = true
    endif
endforeach

if ('lzma2' in get_option('encoders') and not ('lzma1' in get_option('encoders'))) or('lzma2' in get_option('decoders') and not ('lzma1' in get_option('decoders')))
    error('LZMA2 requires that LZMA1 is also enabled')
endif

if get_option('encoders').length() > 0
    config_h.set10('HAVE_ENCODERS', true,
        description: 'Define to 1 if at least one encoder has been enabled.'
    )
endif

if get_option('decoders').length() > 0
    config_h.set10('HAVE_DECODERS', true,
        description: 'Define to 1 if at least one decoder has been enabled.'
    )
endif

### Match finders

match_finder_msg = ''

if 'lzma1' in get_option('encoders') or 'lzma2' in get_option('encoders')
    if get_option('match-finders').length() == 0
        error('at least one match finder is required for an LZ-based encoder')
    endif

    foreach m : get_option('match-finders')
        config_h.set10('HAVE_MF_' + m.underscorify().to_upper(), true,
            description: 'Define to 1 to enable ' + m + ' match finder.'
        )
        match_finder_msg += m + ' '
    endforeach
endif

### Integrity checks

check_msg = ''

if not ('crc32' in get_option('checks'))
    error('For now, the CRC32 check must always be enabled')
endif

foreach c : get_option('checks')
    config_h.set10('HAVE_CHECK_' + c.underscorify().to_upper(), true,
        description: 'Define to 1 if ' + c + ' integrity check is enabled.'
    )
    check_msg += c + ' '
endforeach

### microLZMA

if get_option('microlzma') == true
endif

### .lz (lzip) format support

have_lzip = false
if not ('lzma1' in get_option('encoders'))
    lzip_msg = 'no (LZMA1 disabled)'
elif get_option('lzip-decoder')
    lzip_msg = 'yes'
    have_lzip = true
    config_h.set10('HAVE_LZIP_DECODER', true,
        description: 'Define to 1 if .lz (lzip) decompression support is enabled.'
    )
else
    lzip_msg = 'no'
endif

### assembler optimizations

have_assembler_x86 = false
if get_option('assembler')
   if host_os in asm_os and host_machine.cpu_family() == 'x86'
       have_assembler_x86 = true
   endif
endif

### size optimization

if get_option('small')
    config_h.set10('HAVE_SMALL', true,
        description: 'Define to 1 if optimizing for size.'
    )
endif

### threading

have_threads = false

if get_option('threads') == true
    if sys_windows == true
        if host_machine.cpu_family() == 'x86'
            config_h.set10('MYTHREAD_WIN95', true,
                description: 'Define to 1 when using Windows 95 (and thus XP) compatible threads. This avoids use of features that were added in Windows Vista.'
            )
        else
            config_h.set10('MYTHREAD_VISTA', true,
                description: 'Define to 1 when using Windows Vista compatible threads. This uses features that are not available on Windows XP.'
            )
        endif
        have_threads = true
    else
        pthread_dep = dependency('threads', required : false)
        if pthread_dep.found()
            config_h.set10('MYTHREAD_POSIX', true,
                description: 'Define to 1 when using POSIX threads (pthreads).'
            )
            have_threads = true
        endif
    endif
endif


### subdirectories

subdir('src')

configure_file(output: 'config.h', configuration: config_h)

# summary

summary(
    {
        'OS': host_os,
        'endianness': host_machine.endian(),
        'assembler': get_option('assembler').to_string('yes', 'no'),
        'encoders': encoder_msg,
        'decoders': decoder_msg,
        'match finders': match_finder_msg,
        'checks': check_msg,
        'external sha256': get_option('external-sha256').to_string('yes', 'no'),
        'microLZMA': get_option('microlzma').to_string('yes', 'no'),
        'lzip decompression': lzip_msg,
        'small': get_option('small').to_string('yes', 'no'),
        'threads': have_threads.to_string('yes', 'no'),
        'fast unaligned access': fast_unaligned_access.to_string('yes', 'no'),
        'unsafe type punning': get_option('unsafe-type-punning').to_string('yes', 'no')
    },
    section: 'Configuration Options Summary:',
)
