///////////////////////////////////////////////////////////////////////////////
//
/// \file       tests.h
/// \brief      Common definitions for test applications
//
//  Copyright (C) 2006 Lasse Collin
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_TESTS_H
#define LZMA_TESTS_H

#include "sysdefs.h"
#include "integer.h"

#include <stdio.h>

#define memcrap(buf, size) memset(buf, 0xFD, size)

#define expect(test) ((test) ? 0 : (fprintf(stderr, "%s:%u: %s\n", \
	__FILE__, __LINE__, #test), abort(), 0))

#define succeed(test) expect(!(test))

#define fail(test) expect(test)


static inline const char *
lzma_ret_sym(lzma_ret ret)
{
	if ((unsigned)(ret) > LZMA_PROG_ERROR)
		return "UNKNOWN_ERROR";

	static const char *msgs[] = {
		"LZMA_OK",
		"LZMA_STREAM_END",
		"LZMA_NO_CHECK",
		"LZMA_UNSUPPORTED_CHECK",
		"LZMA_GET_CHECK",
		"LZMA_MEM_ERROR",
		"LZMA_MEMLIMIT_ERROR",
		"LZMA_FORMAT_ERROR",
		"LZMA_HEADER_ERROR",
		"LZMA_DATA_ERROR",
		"LZMA_BUF_ERROR",
		"LZMA_PROG_ERROR"
	};

	return msgs[ret];
}


static inline bool
coder_loop(lzma_stream *strm, uint8_t *in, size_t in_size,
		uint8_t *out, size_t out_size,
		lzma_ret expected_ret, lzma_action finishing_action)
{
	size_t in_left = in_size;
	size_t out_left = out_size > 0 ? out_size + 1 : 0;
	lzma_action action = LZMA_RUN;
	lzma_ret ret;

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = NULL;
	strm->avail_out = 0;

	while (true) {
		if (in_left > 0) {
			if (--in_left == 0)
				action = finishing_action;

			strm->next_in = in++;
			strm->avail_in = 1;
		}

		if (out_left > 0) {
			--out_left;
			strm->next_out = out++;
			strm->avail_out = 1;
		}

		ret = lzma_code(strm, action);
		if (ret != LZMA_OK)
			break;
	}

	bool error = false;

	if (ret != expected_ret)
		error = true;

	if (expected_ret == LZMA_STREAM_END) {
		if (strm->total_in != in_size || strm->total_out != out_size)
			error = true;
	} else {
		if (strm->total_in != in_size || strm->total_out != out_size)
			error = true;
	}

	return error;
}


static inline bool
decoder_loop_ret(lzma_stream *strm, uint8_t *in, size_t in_size,
		lzma_ret expected_ret)
{
	return coder_loop(strm, in, in_size, NULL, 0, expected_ret, LZMA_RUN);
}


static inline bool
decoder_loop(lzma_stream *strm, uint8_t *in, size_t in_size)
{
	return coder_loop(strm, in, in_size, NULL, 0,
			LZMA_STREAM_END, LZMA_RUN);
}

#endif
