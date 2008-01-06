///////////////////////////////////////////////////////////////////////////////
//
/// \file       private.h
/// \brief      Common includes, definions, and prototypes
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

#ifndef PRIVATE_H
#define PRIVATE_H

#include "sysdefs.h"

#ifdef HAVE_ERRNO_H
#	include <errno.h>
#else
extern int errno;
#endif

#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <locale.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "gettext.h"
#define _(msgid) gettext(msgid)
#define N_(msgid1, msgid2, n) ngettext(msgid1, msgid2, n)

#include "alloc.h"
#include "args.h"
#include "error.h"
#include "hardware.h"
#include "help.h"
#include "io.h"
#include "options.h"
#include "process.h"
#include "suffix.h"
#include "util.h"

#endif
