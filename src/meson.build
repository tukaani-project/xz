# SPDX-License-Identifier: 0BSD
# Author: Vincent Torri

subdir('common')
subdir('liblzma')
subdir('xzdec')

if get_option('xz')
    subdir('xz')
endif

if 'lzma1' in get_option('decoders') and get_option('lzmainfo')
    subdir('lzmainfo')
endif

if get_option('scripts')
    subdir('scripts')
endif