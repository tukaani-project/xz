# SPDX-License-Identifier: 0BSD
# Author: Vincent Torri

path_for_script = ''
if get_option('path-for-scripts') == ''
    if sys_sunos
        path_for_script = '/usr/xpg4/bin'
    endif
else
    path_for_script = 'PATH=' + get_option('path-for-scripts') + ':$PATH'
endif

# As discussed on IRC, @xz@ is transformed to 'xz'

scriptconf = configuration_data()

scriptconf.set('PACKAGE_NAME', meson.project_name())
scriptconf.set('PACKAGE_VERSION', meson.project_version())
scriptconf.set('PACKAGE_BUGREPORT', meson.project_version())
scriptconf.set('enable_path_for_scripts', path_for_script)
scriptconf.set('xz', 'xz')

scripts = [
    'xzdiff',
    'xzgrep',
    'xzmore',
    'xzless',
]

foreach s : scripts
    configure_file(
        input : s + '.in',
        output : s,
        configuration : scriptconf,
        install_dir : get_option('bindir')
    )
endforeach

links = [
    [ 'xzdiff', 'xzcmp' ],
    [ 'xzgrep', 'xzegrep' ],
    [ 'xzgrep', 'xzfgrep' ],
]

if get_option('lzma-links')
    links += [
        [ 'xzdiff', 'lzdiff' ],
        [ 'xzdiff', 'lzcmp' ],
        [ 'xzgrep', 'lzgrep' ],
        [ 'xzgrep', 'lzegrep' ],
        [ 'xzgrep', 'lzfgrep' ],
        [ 'xzmore', 'lzmore' ],
        [ 'xzless', 'lzless' ],
    ]
endif

foreach l : links
    install_symlink(l[1],
        install_dir: get_option('bindir'),
        pointing_to: l[0],
    )
endforeach

install_man(files([ 'xzdiff.1', 'xzgrep.1', 'xzless.1', 'xzmore.1' ]))
