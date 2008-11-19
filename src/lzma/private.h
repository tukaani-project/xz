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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

#ifdef ENABLE_NLS
#	include <libintl.h>
#	define _(msgid) gettext(msgid)
#	define N_(msgid1, msgid2, n) ngettext(msgid1, msgid2, n)
#else
#	define _(msgid) (msgid)
#	define N_(msgid1, msgid2, n) ((n) == 1 ? (msgid1) : (msgid2))
#endif

#include "main.h"
#include "process.h"
#include "message.h"
#include "args.h"
#include "hardware.h"
#include "io.h"
#include "options.h"
#include "suffix.h"
#include "util.h"

#endif
