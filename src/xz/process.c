///////////////////////////////////////////////////////////////////////////////
//
/// \file       process.c
/// \brief      Compresses or uncompresses a file
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


enum operation_mode opt_mode = MODE_COMPRESS;

enum format_type opt_format = FORMAT_AUTO;


/// Stream used to communicate with liblzma
static lzma_stream strm = LZMA_STREAM_INIT;

/// Filters needed for all encoding all formats, and also decoding in raw data
static lzma_filter filters[LZMA_FILTERS_MAX + 1];

/// Number of filters. Zero indicates that we are using a preset.
static size_t filters_count = 0;

/// Number of the preset (1-9)
static size_t preset_number = 7;

/// Indicate if no preset has been given. In that case, we will auto-adjust
/// the compression preset so that it doesn't use too much RAM.
// FIXME
static bool preset_default = true;

/// Integrity check type
static lzma_check check = LZMA_CHECK_CRC64;


extern void
coder_set_check(lzma_check new_check)
{
	check = new_check;
	return;
}


extern void
coder_set_preset(size_t new_preset)
{
	preset_number = new_preset;
	preset_default = false;
	return;
}


extern void
coder_add_filter(lzma_vli id, void *options)
{
	if (filters_count == LZMA_FILTERS_MAX)
		message_fatal(_("Maximum number of filters is four"));

	filters[filters_count].id = id;
	filters[filters_count].options = options;
	++filters_count;

	return;
}


extern void
coder_set_compression_settings(void)
{
	// Options for LZMA1 or LZMA2 in case we are using a preset.
	static lzma_options_lzma opt_lzma;

	if (filters_count == 0) {
		// We are using a preset. This is not a good idea in raw mode
		// except when playing around with things. Different versions
		// of this software may use different options in presets, and
		// thus make uncompressing the raw data difficult.
		if (opt_format == FORMAT_RAW) {
			// The message is shown only if warnings are allowed
			// but the exit status isn't changed.
			message(V_WARNING, _("Using a preset in raw mode "
					"is discouraged."));
			message(V_WARNING, _("The exact options of the "
					"presets may vary between software "
					"versions."));
		}

		// Get the preset for LZMA1 or LZMA2.
		if (lzma_lzma_preset(&opt_lzma, preset_number))
			message_bug();

		// Use LZMA2 except with --format=lzma we use LZMA1.
		filters[0].id = opt_format == FORMAT_LZMA
				? LZMA_FILTER_LZMA1 : LZMA_FILTER_LZMA2;
		filters[0].options = &opt_lzma;
		filters_count = 1;
	}

	// Terminate the filter options array.
	filters[filters_count].id = LZMA_VLI_UNKNOWN;

	// If we are using the LZMA_Alone format, allow exactly one filter
	// which has to be LZMA.
	if (opt_format == FORMAT_LZMA && (filters_count != 1
			|| filters[0].id != LZMA_FILTER_LZMA1))
		message_fatal(_("With --format=lzma only the LZMA1 filter "
				"is supported"));

	// TODO: liblzma probably needs an API to validate the filter chain.

	// If using --format=raw, we can be decoding.
	uint64_t memory_usage;
	uint64_t memory_limit;
	if (opt_mode == MODE_COMPRESS) {
		memory_usage = lzma_memusage_encoder(filters);
		memory_limit = hardware_memlimit_encoder();
	} else {
		memory_usage = lzma_memusage_decoder(filters);
		memory_limit = hardware_memlimit_decoder();
	}

	if (memory_usage == UINT64_MAX)
		message_bug();

	if (preset_default) {
		// When no preset was explicitly requested, we use the default
		// preset only if the memory usage limit allows. Otherwise we
		// select a lower preset automatically.
		while (memory_usage > memory_limit) {
			if (preset_number == 1)
				message_fatal(_("Memory usage limit is too "
						"small for any internal "
						"filter preset"));

			if (lzma_lzma_preset(&opt_lzma, --preset_number))
				message_bug();

			memory_usage = lzma_memusage_encoder(filters);
		}
	} else {
		if (memory_usage > memory_limit)
			message_fatal(_("Memory usage limit is too small "
					"for the given filter setup"));
	}

	// Limit the number of worked threads so that memory usage
	// limit isn't exceeded.
	assert(memory_usage > 0);
	size_t thread_limit = memory_limit / memory_usage;
	if (thread_limit == 0)
		thread_limit = 1;

	if (opt_threads > thread_limit)
		opt_threads = thread_limit;

	return;
}


static bool
coder_init(void)
{
	lzma_ret ret = LZMA_PROG_ERROR;

	if (opt_mode == MODE_COMPRESS) {
		switch (opt_format) {
		case FORMAT_AUTO:
			// args.c ensures this.
			assert(0);
			break;

		case FORMAT_XZ:
			ret = lzma_stream_encoder(&strm, filters, check);
			break;

		case FORMAT_LZMA:
			ret = lzma_alone_encoder(&strm, filters[0].options);
			break;

		case FORMAT_RAW:
			ret = lzma_raw_encoder(&strm, filters);
			break;
		}
	} else {
		const uint32_t flags = LZMA_TELL_UNSUPPORTED_CHECK
				| LZMA_CONCATENATED;

		switch (opt_format) {
		case FORMAT_AUTO:
			ret = lzma_auto_decoder(&strm,
					hardware_memlimit_decoder(), flags);
			break;

		case FORMAT_XZ:
			ret = lzma_stream_decoder(&strm,
					hardware_memlimit_decoder(), flags);
			break;

		case FORMAT_LZMA:
			ret = lzma_alone_decoder(&strm,
					hardware_memlimit_decoder());
			break;

		case FORMAT_RAW:
			// Memory usage has already been checked in args.c.
			// FIXME Comment
			ret = lzma_raw_decoder(&strm, filters);
			break;
		}
	}

	if (ret != LZMA_OK) {
		if (ret == LZMA_MEM_ERROR)
			message_error("%s", message_strm(LZMA_MEM_ERROR));
		else
			message_bug();

		return true;
	}

	return false;
}


static bool
coder_run(file_pair *pair)
{
	// Buffers to hold input and output data.
	uint8_t in_buf[IO_BUFFER_SIZE];
	uint8_t out_buf[IO_BUFFER_SIZE];

	// Initialize the progress indicator.
	const uint64_t in_size = pair->src_st.st_size <= (off_t)(0)
			? 0 : (uint64_t)(pair->src_st.st_size);
	message_progress_start(pair->src_name, in_size);

	lzma_action action = LZMA_RUN;
	lzma_ret ret;

	strm.avail_in = 0;
	strm.next_out = out_buf;
	strm.avail_out = IO_BUFFER_SIZE;

	while (!user_abort) {
		// Fill the input buffer if it is empty and we haven't reached
		// end of file yet.
		if (strm.avail_in == 0 && !pair->src_eof) {
			strm.next_in = in_buf;
			strm.avail_in = io_read(pair, in_buf, IO_BUFFER_SIZE);

			if (strm.avail_in == SIZE_MAX)
				break;

			// Encoder needs to know when we have given all the
			// input to it. The decoders need to know it too when
			// we are using LZMA_CONCATENATED.
			if (pair->src_eof)
				action = LZMA_FINISH;
		}

		// Let liblzma do the actual work.
		ret = lzma_code(&strm, action);

		// Write out if the output buffer became full.
		if (strm.avail_out == 0) {
			if (opt_mode != MODE_TEST && io_write(pair, out_buf,
					IO_BUFFER_SIZE - strm.avail_out))
				return false;

			strm.next_out = out_buf;
			strm.avail_out = IO_BUFFER_SIZE;
		}

		if (ret != LZMA_OK) {
			// Determine if the return value indicates that we
			// won't continue coding.
			const bool stop = ret != LZMA_NO_CHECK
					&& ret != LZMA_UNSUPPORTED_CHECK;

			if (stop) {
				// First print the final progress info.
				// This way the user sees more accurately
				// where the error occurred. Note that we
				// print this *before* the possible error
				// message.
				//
				// FIXME: What if something goes wrong
				// after this?
				message_progress_end(strm.total_in,
						strm.total_out,
						ret == LZMA_STREAM_END);

				// Write the remaining bytes even if something
				// went wrong, because that way the user gets
				// as much data as possible, which can be good
				// when trying to get at least some useful
				// data out of damaged files.
				if (opt_mode != MODE_TEST && io_write(pair,
						out_buf, IO_BUFFER_SIZE
							- strm.avail_out))
					return false;
			}

			if (ret == LZMA_STREAM_END) {
				// Check that there is no trailing garbage.
				// This is needed for LZMA_Alone and raw
				// streams.
				if (strm.avail_in == 0 && (pair->src_eof
						|| io_read(pair, in_buf, 1)
							== 0)) {
					assert(pair->src_eof);
					return true;
				}

				// FIXME: What about io_read() failing?

				// We hadn't reached the end of the file.
				ret = LZMA_DATA_ERROR;
				assert(stop);
			}

			// If we get here and stop is true, something went
			// wrong and we print an error. Otherwise it's just
			// a warning and coding can continue.
			if (stop) {
				message_error("%s: %s", pair->src_name,
						message_strm(ret));
			} else {
				message_warning("%s: %s", pair->src_name,
						message_strm(ret));

				// When compressing, all possible errors set
				// stop to true.
				assert(opt_mode != MODE_COMPRESS);
			}

			if (ret == LZMA_MEMLIMIT_ERROR) {
				// Figure out how much memory would have
				// actually needed.
				// TODO
			}

			if (stop)
				return false;
		}

		// Show progress information if --verbose was specified and
		// stderr is a terminal.
		message_progress_update(strm.total_in, strm.total_out);
	}

	return false;
}


extern void
process_file(const char *filename)
{
	// First try initializing the coder. If it fails, it's useless to try
	// opening the file. Check also for user_abort just in case if we had
	// got a signal while initializing the coder.
	if (coder_init() || user_abort)
		return;

	// Try to open the input and output files.
	file_pair *pair = io_open(filename);
	if (pair == NULL)
		return;

	// Do the actual coding.
	const bool success = coder_run(pair);

	// Close the file pair. It needs to know if coding was successful to
	// know if the source or target file should be unlinked.
	io_close(pair, success);

	return;
}
