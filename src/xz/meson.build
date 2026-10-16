# SPDX-License-Identifier: 0BSD
# Author: Vincent Torri

if get_option('assume-ram') <= 0
    error('option "assume-ram" accepts only positive integers')
endif

config_h.set('ASSUME_RAM', get_option('assume-ram'))

# FIXME -DLOCALEDIR=\"$(localedir)\"
xz_cargs = [
    '-DHAVE_CONFIG_H',
]

xz_incdir = [
    '../common',
    config_dir,
]

xz_src = [
    'args.c',
    'coder.c',
    'file_io.c',
    'hardware.c',
    'main.c',
    'message.c',
    'mytime.c',
    'options.c',
    'sandbox.c',
    'signals.c',
    'suffix.c',
    'util.c',
    '../common/tuklib_open_stdxxx.c',
    '../common/tuklib_progname.c',
    '../common/tuklib_exit.c',
    '../common/tuklib_mbstr_width.c',
    '../common/tuklib_mbstr_fw.c',
]

if get_option('decoders').length() > 0
    xz_src += [
        'list.c'
    ]
endif

if sys_windows
    xz_rc = win_mod.compile_resources(files('xz_w32res.rc'),
        args : xz_cargs,
        depend_files : files('../common/common_w32res.rc'),
        include_directories : [ '../src/common/', config_dir ]
    )
    xz_src += [ xz_rc ]
endif

executable('xz', xz_src,
    c_args: xz_cargs,
    dependencies: lzma_dep,
    include_directories : xz_incdir,
    install: true,
)

install_man(files('xz.1'))

xz_links = [
    'unxz',
    'xzcat',
]

if get_option('lzma-links')
    xz_links += [
        'lzma',
        'unlzma',
        'lzcat',
    ]
endif


foreach l : xz_links
    install_symlink(l + (sys_windows ? '.exe' : ''),
        install_dir: get_option('bindir'),
        pointing_to: 'xz' + (sys_windows ? '.exe' : ''),
    )
endforeach
