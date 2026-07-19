#!/bin/sh
# SPDX-License-Identifier: 0BSD

###############################################################################
#
# Test that xz uses a Linux cgroup v2 memory.max and cpu.max
#
###############################################################################

test -f /proc/self/cgroup || exit 77
test -f /sys/fs/cgroup/cgroup.controllers || exit 77
command -v systemd-run > /dev/null 2>&1 || exit 77

# systemd version 236 or later is needed for --collect.
systemd_version=$(systemd-run --version \
		| sed -n '1s/^systemd \([0-9]*\) .*$/\1/p')
test "$systemd_version" -ge 236 || exit 77

XZ=${1:-../src/xz/xz}
test -x "$XZ" || exit 77

# Use low enough MemoryMax so that the test works on a system with
# a low amount of RAM.
MEM=16
SYSTEMD_RUN="systemd-run --user --quiet --wait --pipe --collect -p MemoryMax=${MEM}M"

# A transient systemd unit gives the test its own cgroup without changing the
# host's cgroup hierarchy. --pipe is needed to capture xz's output.
output=$($SYSTEMD_RUN "$XZ" -M0 --robot --info-memory) || exit 77

# Remember the number of threads when CPUQuota isn't set.
default_threads=$(printf '%s\n' "$output" | cut -f6)

# The fifth column is the default memory usage limit for -T0. Keep a margin
# below the cgroup limit because xz itself and allocation overhead also need
# memory.
default_memlimit=$(printf '%s\n' "$output" | cut -f5)
test -n "$default_memlimit" || exit 1
test "$default_memlimit" -le $(($MEM * 1024 * 1024 / 4)) || {
	echo "xz ignored the cgroup memory limit: $output"
	exit 1
}

# Percentage limits use the same memory basis as automatic limits.
# Specify CPUQuota to test cpu.max at the same time.
output=$($SYSTEMD_RUN -p CPUQuota=277% \
	"$XZ" -M0 --memlimit-compress=50% --robot --info-memory) || exit 77
compression_memlimit=$(printf '%s\n' "$output" | cut -f2)
test "$compression_memlimit" -eq $(($MEM * 1024 * 1024 / 2)) || {
	echo "xz ignored the cgroup memory limit for percentages: $output"
	exit 1
}

limited_threads=$(printf '%s\n' "$output" | cut -f6)
test "$limited_threads" -eq 2 || test "$default_threads" -eq 1 || {
	echo "xz ignored the cgroup CPU limit: $output"
	exit 1
}

exit 0
