# SPDX-License-Identifier: 0BSD
# Author: Vincent Torri

xz_test_src = [
    'create_compress_files',
    'test_check',
    'test_hardware',
    'test_stream_flags',
    'test_filter_flags',
    'test_filter_str',
    'test_block_header',
    'test_index',
    'test_index_hash',
    'test_bcj_exact_size',
    'test_memlimit',
    'test_lzip_decoder',
    'test_vli',
]
if get_option('microlzma')
    xz_test_src += [
        'test_microlzma',
    ]
endif

test_cargs = [
    '-DHAVE_CONFIG_H',
]

test_incdir = [
    '../src/common',
    '../src/liblzma',
    '../src/liblzma/api',
    config_dir
]

foreach t : xz_test_src
    exe = executable(
        t,
        files(t + '.c'),
        c_args: test_cargs,
        dependencies: lzma_dep,
        include_directories: test_incdir,
    )
    test(t, exe, timeout: 0, workdir: meson.current_source_dir())
endforeach

# TODO: script ones
