///////////////////////////////////////////////////////////////////////////////
//
/// \file       help.c
/// \brief      Help messages
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"


extern void
show_try_help(void)
{
	// Print this with V_WARNING instead of V_ERROR to prevent it from
	// showing up when --quiet has been specified.
	errmsg(V_WARNING, _("Try `%s --help' for more information."), argv0);
	return;
}


extern void lzma_attribute((noreturn))
show_help(void)
{
	printf(_("Usage: %s [OPTION]... [FILE]...\n"
			"Compress or decompress FILEs in the .lzma format.\n"
			"\n"), argv0);

	puts(_("Mandatory arguments to long options are mandatory for "
			"short options too.\n"));

	puts(_(
" Operation mode:\n"
"\n"
"  -z, --compress      force compression\n"
"  -d, --decompress    force decompression\n"
"  -t, --test          test compressed file integrity\n"
"  -l, --list          list block sizes, total sizes, and possible metadata\n"
));

	puts(_(
" Operation modifiers:\n"
"\n"
"  -k, --keep          keep (don't delete) input files\n"
"  -f, --force         force overwrite of output file and (de)compress links\n"
"  -c, --stdout        write to standard output and don't delete input files\n"
"  -S, --suffix=.SUF   use suffix `.SUF' on compressed files instead of `.lzma'\n"
"  -F, --format=FMT    file format to encode or decode; possible values are\n"
"                      `auto', `native', `single', `multi', and `alone'\n"
"      --files=[FILE]  read filenames to process from FILE; if FILE is\n"
"                      omitted, filenames are read from the standard input;\n"
"                      filenames must be terminated with the newline character\n"
"      --files0=[FILE] like --files but use the nul byte as terminator\n"
));

	puts(_(
" Compression presets and basic compression options:\n"
"\n"
"  -1 .. -2            fast compression\n"
"  -3 .. -6            good compression\n"
"  -7 .. -9            excellent compression, but needs a lot of memory;\n"
"                      default is -7 if memory limit allows\n"
"\n"
"  -C, --check=CHECK   integrity check type: `crc32', `crc64' (default),\n"
"                      or `sha256'\n"
));

	puts(_(
" Custom filter chain for compression (alternative for using presets):\n"
"\n"
"  --lzma=[OPTS]       LZMA filter; OPTS is a comma-separated list of zero or\n"
"                      more of the following options (valid values; default):\n"
"                        dict=NUM   dictionary size in bytes (1 - 1Gi; 8Mi)\n"
"                        lc=NUM     number of literal context bits (0-8; 3)\n"
"                        lp=NUM     number of literal position bits (0-4; 0)\n"
"                        pb=NUM     number of position bits (0-4; 2)\n"
"                        mode=MODE  compression mode (`fast' or `best'; `best')\n"
"                        fb=NUM     number of fast bytes (5-273; 128)\n"
"                        mf=NAME    match finder (hc3, hc4, bt2, bt3, bt4; bt4)\n"
"                        mfc=NUM    match finder cycles; 0=automatic (default)\n"
"\n"
"  --x86               x86 filter (sometimes called BCJ filter)\n"
"  --powerpc           PowerPC (big endian) filter\n"
"  --ia64              IA64 (Itanium) filter\n"
"  --arm               ARM filter\n"
"  --armthumb          ARM-Thumb filter\n"
"  --sparc             SPARC filter\n"
"\n"
"  --copy              No filtering (useful only when specified alone)\n"
"  --subblock=[OPTS]   Subblock filter; valid OPTS (valid values; default):\n"
"                        size=NUM    number of bytes of data per subblock\n"
"                                    (1 - 256Mi; 4Ki)\n"
"                        rle=NUM     run-length encoder chunk size (0-256; 0)\n"
));

/*
These aren't implemented yet.

	puts(_(
" Metadata options:\n"
"\n"
"  -N, --name          save or restore the original filename and time stamp\n"
"  -n, --no-name       do not save or restore filename and time stamp (default)\n"
"  -S, --sign=KEY      sign the data with GnuPG when compressing, or verify\n"
"                      the signature when decompressing\n"));
*/

	puts(_(
" Resource usage options:\n"
"\n"
"  -M, --memory=NUM    use roughly NUM bytes of memory at maximum\n"
"  -T, --threads=NUM   use at maximum of NUM (de)compression threads\n"
// "      --threading=STR threading style; possible values are `auto' (default),\n"
// "                      `files', and `stream'
));

	puts(_(
" Other options:\n"
"\n"
"  -q, --quiet         suppress warnings; specify twice to suppress errors too\n"
"  -v, --verbose       be verbose; specify twice for even more verbose\n"
"\n"
"  -h, --help          display this help and exit\n"
"  -V, --version       display version and license information and exit\n"));

	puts(_("With no FILE, or when FILE is -, read standard input.\n"));

	size_t mem_limit = opt_memory / (1024 * 1024);
	if (mem_limit == 0)
		mem_limit = 1;

	puts(_("On this system and configuration, the tool will use"));
	printf(_("  * roughly %zu MiB of memory at maximum; and\n"),
			mem_limit);
	printf(N_(
		"  * at maximum of one thread for (de)compression.\n\n",
		"  * at maximum of %zu threads for (de)compression.\n\n",
		opt_threads), opt_threads);

	printf(_("Report bugs to <%s> (in English or Finnish).\n"),
			PACKAGE_BUGREPORT);

	my_exit(SUCCESS);
}


extern void lzma_attribute((noreturn))
show_version(void)
{
	printf(
"lzma (LZMA Utils) " PACKAGE_VERSION "\n"
"\n"
"Copyright (C) 1999-2006 Igor Pavlov\n"
"Copyright (C) 2007 Lasse Collin\n"
"\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n");
	my_exit(SUCCESS);
}
