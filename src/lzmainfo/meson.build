# SPDX-License-Identifier: 0BSD
# Author: Vincent Torri

lzmainfo_cargs = [
    '-DHAVE_CONFIG_H',
]

lzmainfo_incdir = [
    '../common',
    config_dir,
]

lzmainfo_src = [
    'lzmainfo.c',
    '../common/tuklib_progname.c',
    '../common/tuklib_exit.c',
]

if sys_windows
    lzmainfo_rc = win_mod.compile_resources(files('lzmainfo_w32res.rc'),
        args : lzmainfo_cargs,
        depend_files : files('../common/common_w32res.rc'),
        include_directories : [ '../src/common/', config_dir ]
    )
    lzmainfo_src += [ lzmainfo_rc ]
endif

executable('lzmainfo', lzmainfo_src,
    c_args: lzmainfo_cargs,
    dependencies: lzma_dep,
    include_directories : lzmainfo_incdir,
    install: true,
)

install_man(files('lzmainfo.1'))
