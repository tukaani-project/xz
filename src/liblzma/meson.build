## SPDX-License-Identifier: 0BSD
## Author: Vincent Torri

lzma_src = [
    '../common/tuklib_physmem.c',
]

if have_threads
    lzma_src += [
        '../common/tuklib_cpucores.c',
    ]
endif

# common/Makefile.inc

lzma_src += [
    'common/common.c',
    'common/block_util.c',
    'common/easy_preset.c',
    'common/filter_common.c',
    'common/hardware_physmem.c',
    'common/index.c',
    'common/stream_flags_common.c',
    'common/string_conversion.c',
    'common/vli_size.c',
]

if have_threads
    lzma_src += [
        'common/hardware_cputhreads.c',
        'common/outqueue.c',
    ]
endif

if get_option('encoders').length() > 0
    lzma_src += [
        'common/alone_encoder.c',
        'common/block_buffer_encoder.c',
        'common/block_encoder.c',
        'common/block_header_encoder.c',
        'common/easy_buffer_encoder.c',
        'common/easy_encoder.c',
        'common/easy_encoder_memusage.c',
        'common/filter_buffer_encoder.c',
        'common/filter_encoder.c',
        'common/filter_flags_encoder.c',
        'common/index_encoder.c',
        'common/stream_buffer_encoder.c',
        'common/stream_encoder.c',
        'common/stream_flags_encoder.c',
        'common/vli_encoder.c',
    ]

    if have_threads
        lzma_src += [
            'common/stream_encoder_mt.c',
        ]
    endif

    if get_option('microlzma')
        lzma_src += [
            'common/microlzma_encoder.c',
        ]
    endif
endif

if get_option('decoders').length() > 0
    lzma_src += [
        'common/alone_decoder.c',
        'common/auto_decoder.c',
        'common/block_buffer_decoder.c',
        'common/block_decoder.c',
        'common/block_header_decoder.c',
        'common/easy_decoder_memusage.c',
        'common/file_info.c',
        'common/filter_buffer_decoder.c',
        'common/filter_decoder.c',
        'common/filter_flags_decoder.c',
        'common/index_decoder.c',
        'common/index_hash.c',
        'common/stream_buffer_decoder.c',
        'common/stream_decoder.c',
        'common/stream_flags_decoder.c',
        'common/vli_decoder.c'
    ]

    if have_threads
        lzma_src += [
            'common/stream_decoder_mt.c',
        ]
    endif

    if get_option('microlzma')
        lzma_src += [
            'common/microlzma_decoder.c'
        ]
    endif

    if have_lzip
        lzma_src += 'common/lzip_decoder.c'
    endif
endif

# check/Makefile.inc

lzma_src += [
    'check/check.c'
]

if get_option('small')
    lzma_src += [
        'check/crc32_small.c'
    ]
else
    lzma_src += [
        'check/crc32_table.c'
    ]

    if have_assembler_x86
        lzma_src += [
            'check/crc32_x86.S'
        ]
    else
        lzma_src += [
            'check/crc32_fast.c'
        ]
    endif
endif

if 'crc64' in get_option('checks')
    if get_option('small')
        lzma_src += [
            'check/crc64_small.c'
        ]
    else
        lzma_src += [
            'check/crc64_table.c'
        ]

        if have_assembler_x86
            lzma_src += [
                'check/crc64_x86.S'
            ]
        else
            lzma_src += [
                'check/crc64_fast.c'
            ]
        endif
    endif
endif

if ('sha256' in get_option('checks')) and (not get_option('external-sha256'))
    lzma_src += [
        'check/sha256.c'
    ]
endif

# lz/Makefile.inc
if ('lzma1' in get_option('encoders')) or ('lzma2' in get_option('encoders'))
    lzma_src += [
        'lz/lz_encoder.c',
        'lz/lz_encoder_mf.c'
    ]
 endif

if ('lzma1' in get_option('decoders')) or ('lzma2' in get_option('decoders'))
    lzma_src += [
        'lz/lz_decoder.c'
    ]
endif

if ('lzma1' in get_option('encoders')) or ('lzma1' in get_option('decoders'))

    # lzma/Makefile.inc
    lzma_src += [
        'lzma/lzma_encoder_presets.c'
    ]

    if 'lzma1' in get_option('encoders')
        lzma_src += [
            'lzma/lzma_encoder.c',
            'lzma/lzma_encoder_optimum_fast.c',
            'lzma/lzma_encoder_optimum_normal.c'
        ]

        if not get_option('small')
            lzma_src += [
                'lzma/fastpos_table.c'
            ]
        endif
    endif

    if 'lzma1' in get_option('decoders')
        lzma_src += [
            'lzma/lzma_decoder.c'
        ]
    endif

    if 'lzma2' in get_option('encoders')
        lzma_src += [
            'lzma/lzma2_encoder.c'
        ]
    endif

    if 'lzma2' in get_option('decoders')
        lzma_src += [
            'lzma/lzma2_decoder.c'
        ]
    endif

    # rangecoder/Makefile.inc
    if 'lzma1' in get_option('encoders')
        lzma_src += [
            'rangecoder/price_table.c'
        ]
    endif
endif

if ('delta' in get_option('encoders')) or ('delta' in get_option('decoders'))

    # delta/Makefile.inc
    lzma_src += [
        'delta/delta_common.c'
    ]

    if 'delta' in get_option('encoders')
        lzma_src += [
            'delta/delta_encoder.c'
        ]
    endif

    if 'delta' in get_option('decoders')
        lzma_src += [
            'delta/delta_decoder.c'
        ]
    endif
endif

if have_encoder_simple_filters or have_decoder_simple_filters

    # simple/Makefile.inc
    lzma_src += [
        'simple/simple_coder.c'
    ]

    if have_encoder_simple_filters
        lzma_src += [
            'simple/simple_encoder.c'
        ]
    endif

    if have_decoder_simple_filters
        lzma_src += [
            'simple/simple_decoder.c'
        ]
    endif

    foreach f : simple_filters
        if (f in get_option('encoders')) or
           (f in get_option('decoders'))
            lzma_src += [
                'simple/' + f + '.c'
            ]
        endif
    endforeach
endif

# resource file

win_mod = import('windows')

if sys_windows
    liblzma_rc = win_mod.compile_resources(files('liblzma_w32res.rc'),
        args : [ '-DHAVE_CONFIG_H', '-DTUKLIB_SYMBOL_PREFIX=lzma_' ],
        depend_files : files('../common/common_w32res.rc'),
        include_directories : [ '../src/common/', config_dir ]
    )
    lzma_src += [ liblzma_rc ]
endif

lzma_c_args += [
    '-DHAVE_CONFIG_H',
]

lzma_incdir = [
    config_dir,
    '../common',
    'api',
    'check',
    'common',
    'delta',
    'lz',
    'lzma',
    'rangecoder',
    'simple',
]

lzma_deps = []

if (not sys_windows) and have_threads
    lzma_deps += pthread_dep
endif

lzma_lib = library('lzma', lzma_src,
    c_args : lzma_c_args,
    dependencies : lzma_deps,
    gnu_symbol_visibility : 'hidden',
    include_directories : lzma_incdir,
    install : true,
    link_args : sys_windows ? '-Wl,--output-def,liblzma.def.in' : '',
    version : meson.project_version(),
)

lzma_dep = declare_dependency(link_with: lzma_lib)

# pc file

pkg_mod = import('pkgconfig')

pkg_mod.generate(
    lzma_lib,
    description : 'General purpose data compression library',
    name : 'liblzma',
    url : 'https://tukaani.org/xz/',
)

install_headers('api/lzma.h')

lzma_headers = files([
    'api/lzma/base.h',
    'api/lzma/bcj.h',
    'api/lzma/block.h',
    'api/lzma/check.h',
    'api/lzma/container.h',
    'api/lzma/delta.h',
    'api/lzma/filter.h',
    'api/lzma/hardware.h',
    'api/lzma/index.h',
    'api/lzma/index_hash.h',
    'api/lzma/lzma12.h',
    'api/lzma/stream_flags.h',
    'api/lzma/version.h',
    'api/lzma/vli.h',
])

install_headers(lzma_headers, subdir: 'lzma'
)
