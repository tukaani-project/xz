#!/bin/sh
# SPDX-License-Identifier: 0BSD

###############################################################################
#
# Test that xz uses a Linux cgroup v2 memory limit when selecting its default
# memory usage limit.
#
###############################################################################

test -f /proc/self/cgroup || exit 77
test -f /sys/fs/cgroup/cgroup.controllers || exit 77
command -v systemd-run > /dev/null 2>&1 || exit 77

XZ=${1:-../src/xz/xz}
test -x "$XZ" || exit 77

# A transient systemd unit gives the test its own cgroup without changing the
# host's cgroup hierarchy. --pipe is needed to capture xz's output. Some test
# hosts require root to create transient units; use passwordless sudo there.
SYSTEMD_RUN="systemd-run"
if ! $SYSTEMD_RUN --quiet --wait --pipe --collect -p MemoryMax=256M \
	"$XZ" --robot --info-memory < /dev/null > /dev/null 2>&1; then
	if sudo -n true > /dev/null 2>&1; then
		SYSTEMD_RUN="sudo -n systemd-run"
	else
		exit 77
	fi
fi

output=$($SYSTEMD_RUN --quiet --wait --pipe --collect \
	-p MemoryMax=256M "$XZ" --robot --info-memory < /dev/null) || exit 77

# The fifth column is the default memory usage limit for -T0. Keep a margin
# below the cgroup limit because xz itself and allocation overhead also need
# memory.
default_limit=$(printf '%s\n' "$output" | cut -f5)
test -n "$default_limit" || exit 1
test "$default_limit" -le $((256 * 1024 * 1024 / 4)) || {
	echo "xz ignored the cgroup memory limit: $output" >&2
	exit 1
}

# Percentage limits use the same memory basis as automatic limits.
output=$($SYSTEMD_RUN --quiet --wait --pipe --collect \
	-p MemoryMax=256M "$XZ" --memlimit-compress=50% --robot \
	--info-memory < /dev/null) || exit 77
compression_limit=$(printf '%s\n' "$output" | cut -f2)
test "$compression_limit" = $((256 * 1024 * 1024 / 2)) || {
	echo "xz ignored the cgroup memory limit for percentages: $output" >&2
	exit 1
}

exit 0
