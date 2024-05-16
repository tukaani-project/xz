## SPDX-License-Identifier: 0BSD
## Author: Vincent Torri

xzdec_cargs = [
    '-DHAVE_CONFIG_H',
    '-DHAVE_STDBOOL_H',
    '-DHAVE_INTTYPES_H',
    '-DHAVE_STDINT_H',
    '-DTUKLIB_GETTEXT=0'
]

if get_option('decoders').length() > 0 and get_option('xzdec')
    xzdec_src = [
        '../common/tuklib_exit.c',
        '../common/tuklib_progname.c',
        'xzdec.c'
    ]

    xzdec_incdir = [
        '../common',
        config_dir,
    ]

    if sys_windows
        xzdec_rc = win_mod.compile_resources(files('xzdec_w32res.rc'),
            args : xzdec_cargs,
            depend_files : files('../common/common_w32res.rc'),
            include_directories : [ '../src/common/', config_dir ]
        )
        xzdec_src += [ xzdec_rc ]
    endif

    executable('xzdec', xzdec_src,
        c_args: xzdec_cargs,
        dependencies: lzma_dep,
        include_directories : xzdec_incdir,
        install: true,
    )

    install_man(files('xzdec.1'))
endif

if 'lzma1' in get_option('decoders') and get_option('lzmadec')
    lzmadec_src = [
        '../common/tuklib_exit.c',
        '../common/tuklib_progname.c',
        'xzdec.c'
    ]

    lzmadec_cargs = [
        '-DHAVE_CONFIG_H',
        '-DHAVE_STDBOOL_H',
        '-DHAVE_INTTYPES_H',
        '-DHAVE_STDINT_H',
        '-DTUKLIB_GETTEXT=0'
    ]

    lzmadec_incdir = [
        '../common',
        config_dir,
    ]

    if sys_windows
        lzmadec_rc = win_mod.compile_resources(files('lzmadec_w32res.rc'),
            args : [ xzdec_cargs, '-DLZMADEC' ],
            depend_files : files('../common/common_w32res.rc'),
            include_directories : [ '../src/common/', config_dir ]
        )
        lzmadec_src += [ lzmadec_rc ]
    endif

    executable('lzmadec', lzmadec_src,
        c_args: lzmadec_cargs,
        dependencies: lzma_dep,
        include_directories : lzmadec_incdir,
        install: true,
    )

    if get_option('xzdec')
        install_symlink('lzmadec.1',
            install_dir: get_option('datadir') / 'man' / 'man1',
            pointing_to: 'xzdec.1',
        )
    else
        fs = import('fs')
        fs.copyfile('xzdec.1', 'lzmadec.1')
        install_man('lzmadec.1')
    endif
endif
