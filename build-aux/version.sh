#!/bin/sh
# SPDX-License-Identifier: 0BSD

#############################################################################
#
# Get the version string from version.h and print it out without
# trailing newline. This makes it suitable for use in configure.ac.
#
#############################################################################
#
# Author: Lasse Collin
#         Kerin Millar
#
#############################################################################

# Solaris provides a legacy awk implementation as /usr/bin/awk. It also provides
# /usr/bin/nawk, which is more or less POSIX-compliant. Be sure to use it.
if grep Solaris /etc/release >/dev/null 2>&1; then
	awk=nawk
else
	awk=awk
fi

"$awk" -f - <<'EOF'

BEGIN {
	ARGV[1] = "src/liblzma/api/lzma/version.h"
	ARGC = 2
	OFS = "."
	ORS = ""
}

/^#define LZMA_VERSION_(MAJOR|MINOR|PATCH|STABILITY) / {
	sub(/.*_/, "", $2)
	sub(/.*_/, "", $3)
	if ($2 != "STABILITY" || $3 != "STABLE") {
		ver[$2] = tolower($3)
	}
}

END {
	print ver["MAJOR"], ver["MINOR"], ver["PATCH"] ver["STABILITY"]
}

EOF
