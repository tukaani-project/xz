///////////////////////////////////////////////////////////////////////////////
//
/// \file       code.c
/// \brief      zlib-like API wrapper for liblzma's internal API
//
//  Copyright (C) 2007 Lasse Collin
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


LZMA_API const lzma_stream LZMA_STREAM_INIT_VAR = {
	.next_in = NULL,
	.avail_in = 0,
	.total_in = 0,
	.next_out = NULL,
	.avail_out = 0,
	.total_out = 0,
	.allocator = NULL,
	.internal = NULL,
};


extern lzma_ret
lzma_strm_init(lzma_stream *strm)
{
	if (strm == NULL)
		return LZMA_PROG_ERROR;

	if (strm->internal == NULL) {
		strm->internal = lzma_alloc(sizeof(lzma_internal),
				strm->allocator);
		if (strm->internal == NULL)
			return LZMA_MEM_ERROR;

		strm->internal->next = LZMA_NEXT_CODER_INIT;
	}

	strm->internal->supported_actions[LZMA_RUN] = false;
	strm->internal->supported_actions[LZMA_SYNC_FLUSH] = false;
	strm->internal->supported_actions[LZMA_FULL_FLUSH] = false;
	strm->internal->supported_actions[LZMA_FINISH] = false;
	strm->internal->sequence = ISEQ_RUN;

	strm->total_in = 0;
	strm->total_out = 0;

	return LZMA_OK;
}


extern LZMA_API lzma_ret
lzma_code(lzma_stream *strm, lzma_action action)
{
	// Sanity checks
	if ((strm->next_in == NULL && strm->avail_in != 0)
			|| (strm->next_out == NULL && strm->avail_out != 0)
			|| strm->internal == NULL
			|| strm->internal->next.code == NULL
			|| (unsigned int)(action) > LZMA_FINISH
			|| !strm->internal->supported_actions[action])
		return LZMA_PROG_ERROR;

	switch (strm->internal->sequence) {
	case ISEQ_RUN:
		switch (action) {
		case LZMA_RUN:
			break;

		case LZMA_SYNC_FLUSH:
			strm->internal->sequence = ISEQ_SYNC_FLUSH;
			break;

		case LZMA_FULL_FLUSH:
			strm->internal->sequence = ISEQ_FULL_FLUSH;
			break;

		case LZMA_FINISH:
			strm->internal->sequence = ISEQ_FINISH;
			break;
		}

		break;

	case ISEQ_SYNC_FLUSH:
		if (action != LZMA_SYNC_FLUSH)
			return LZMA_PROG_ERROR;

		// Check that application doesn't change avail_in once
		// LZMA_SYNC_FLUSH has been used.
		if (strm->internal->avail_in != strm->avail_in)
			return LZMA_DATA_ERROR;

		break;

	case ISEQ_FULL_FLUSH:
		if (action != LZMA_FULL_FLUSH)
			return LZMA_PROG_ERROR;

		// Check that application doesn't change avail_in once
		// LZMA_FULL_FLUSH has been used.
		if (strm->internal->avail_in != strm->avail_in)
			return LZMA_DATA_ERROR;

		break;

	case ISEQ_FINISH:
		if (action != LZMA_FINISH)
			return LZMA_PROG_ERROR;

		if (strm->internal->avail_in != strm->avail_in)
			return LZMA_DATA_ERROR;

		break;

	case ISEQ_END:
		return LZMA_STREAM_END;

	case ISEQ_ERROR:
	default:
		return LZMA_PROG_ERROR;
	}

	size_t in_pos = 0;
	size_t out_pos = 0;
	lzma_ret ret = strm->internal->next.code(
			strm->internal->next.coder, strm->allocator,
			strm->next_in, &in_pos, strm->avail_in,
			strm->next_out, &out_pos, strm->avail_out, action);

	strm->next_in += in_pos;
	strm->avail_in -= in_pos;
	strm->total_in += in_pos;

	strm->next_out += out_pos;
	strm->avail_out -= out_pos;
	strm->total_out += out_pos;

	strm->internal->avail_in = strm->avail_in;

	switch (ret) {
	case LZMA_OK:
		// Don't return LZMA_BUF_ERROR when it happens the first time.
		// This is to avoid returning LZMA_BUF_ERROR when avail_out
		// was zero but still there was no more data left to written
		// to next_out.
		if (out_pos == 0 && in_pos == 0) {
			if (strm->internal->allow_buf_error)
				ret = LZMA_BUF_ERROR;
			else
				strm->internal->allow_buf_error = true;
		} else {
			strm->internal->allow_buf_error = false;
		}
		break;

	case LZMA_STREAM_END:
		if (strm->internal->sequence == ISEQ_SYNC_FLUSH
				|| strm->internal->sequence == ISEQ_FULL_FLUSH)
			strm->internal->sequence = ISEQ_RUN;
		else
			strm->internal->sequence = ISEQ_END;
		break;

	case LZMA_UNSUPPORTED_CHECK:
		strm->internal->allow_buf_error = false;
		break;

	default:
		// All the other errors are fatal; coding cannot be continued.
		strm->internal->sequence = ISEQ_ERROR;
		break;
	}

	return ret;
}


extern LZMA_API void
lzma_end(lzma_stream *strm)
{
	if (strm != NULL && strm->internal != NULL) {
		if (strm->internal->next.end != NULL)
			strm->internal->next.end(strm->internal->next.coder,
					strm->allocator);

		lzma_free(strm->internal, strm->allocator);
		strm->internal = NULL;
	}

	return;
}
