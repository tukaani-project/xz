#!/bin/bash
# SPDX-License-Identifier: 0BSD

# Convert certain invisible BiDi control characters to/from ASCII strings
# that shouldn't otherwise occur in translation files. Conversion is done
# in-place using GNU sed -i.

export LC_ALL=en_US.UTF-8

case $1 in
    po-to-ascii)
        sed -i $'
            s/\u200e/{LRM}/g
            s/\u200f/{RLM}/g
            s/\u2066/{LRI}/g
            s/\u2067/{RLI}/g
            s/\u2068/{FSI}/g
            s/\u2069/{PDI}/g
            ' -- "$2"
        ;;
    ascii-to-po)
        sed -i $'
            s/{LRM}/\u200e/g
            s/{RLM}/\u200f/g
            s/{LRI}/\u2066/g
            s/{RLI}/\u2067/g
            s/{FSI}/\u2068/g
            s/{PDI}/\u2069/g
            ' -- "$2"
        ;;
    *)
        echo "Usage: $0 <po-to-ascii|ascii-to-po> file.po" >&2
        exit 1
        ;;
esac
