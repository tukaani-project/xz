///////////////////////////////////////////////////////////////////////////////
//
/// \file       process.c
/// \brief      Compresses or uncompresses a file
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
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

/// Number of the preset (0-9)
static size_t preset_number = 6;

/// True if we should auto-adjust the compression settings to use less memory
/// if memory usage limit is too low for the original settings.
static bool auto_adjust = true;

/// Indicate if no preset has been explicitly given. In that case, if we need
/// to auto-adjust for lower memory usage, we won't print a warning.
static bool preset_default = true;

/// If a preset is used (no custom filter chain) and preset_extreme is true,
/// a significantly slower compression is used to achieve slightly better
/// compression ratio.
static bool preset_extreme = false;

/// Integrity check type
#ifdef HAVE_CHECK_CRC64
static lzma_check check = LZMA_CHECK_CRC64;
#else
static lzma_check check = LZMA_CHECK_CRC32;
#endif


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
coder_set_extreme(void)
{
	preset_extreme = true;
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


static void lzma_attribute((noreturn))
memlimit_too_small(uint64_t memory_usage, uint64_t memory_limit)
{
	message_fatal(_("Memory usage limit (%" PRIu64 " MiB) is too small "
			"for the given filter setup (%" PRIu64 " MiB)"),
			memory_limit >> 20, memory_usage >> 20);
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
		if (preset_extreme)
			preset_number |= LZMA_PRESET_EXTREME;

		if (lzma_lzma_preset(&opt_lzma, preset_number))
			message_bug();

		// Use LZMA2 except with --format=lzma we use LZMA1.
		filters[0].id = opt_format == FORMAT_LZMA
				? LZMA_FILTER_LZMA1 : LZMA_FILTER_LZMA2;
		filters[0].options = &opt_lzma;
		filters_count = 1;
	} else {
		preset_default = false;
	}

	// Terminate the filter options array.
	filters[filters_count].id = LZMA_VLI_UNKNOWN;

	// If we are using the LZMA_Alone format, allow exactly one filter
	// which has to be LZMA.
	if (opt_format == FORMAT_LZMA && (filters_count != 1
			|| filters[0].id != LZMA_FILTER_LZMA1))
		message_fatal(_("With --format=lzma only the LZMA1 filter "
				"is supported"));

	// Print the selected filter chain.
	message_filters(V_DEBUG, filters);

	// If using --format=raw, we can be decoding. The memusage function
	// also validates the filter chain and the options used for the
	// filters.
	const uint64_t memory_limit = hardware_memlimit_get();
	uint64_t memory_usage;
	if (opt_mode == MODE_COMPRESS)
		memory_usage = lzma_raw_encoder_memusage(filters);
	else
		memory_usage = lzma_raw_decoder_memusage(filters);

	if (memory_usage == UINT64_MAX)
		message_fatal("Unsupported filter chain or filter options");

	// Print memory usage info.
	message(V_DEBUG, _("%s MiB (%s B) of memory is required per thread, "
			"limit is %s MiB (%s B)"),
			uint64_to_str(memory_usage >> 20, 0),
			uint64_to_str(memory_usage, 1),
			uint64_to_str(memory_limit >> 20, 2),
			uint64_to_str(memory_limit, 3));

	if (memory_usage > memory_limit) {
		// If --no-auto-adjust was used or we didn't find LZMA1 or
		// LZMA2 as the last filter, give an error immediatelly.
		// --format=raw implies --no-auto-adjust.
		if (!auto_adjust || opt_format == FORMAT_RAW)
			memlimit_too_small(memory_usage, memory_limit);

		assert(opt_mode == MODE_COMPRESS);

		// Look for the last filter if it is LZMA2 or LZMA1, so
		// we can make it use less RAM. With other filters we don't
		// know what to do.
		size_t i = 0;
		while (filters[i].id != LZMA_FILTER_LZMA2
				&& filters[i].id != LZMA_FILTER_LZMA1) {
			if (filters[i].id == LZMA_VLI_UNKNOWN)
				memlimit_too_small(memory_usage, memory_limit);

			++i;
		}

		// Decrease the dictionary size until we meet the memory
		// usage limit. First round down to full mebibytes.
		lzma_options_lzma *opt = filters[i].options;
		const uint32_t orig_dict_size = opt->dict_size;
		opt->dict_size &= ~((UINT32_C(1) << 20) - 1);
		while (true) {
			// If it is below 1 MiB, auto-adjusting failed. We
			// could be more sophisticated and scale it down even
			// more, but let's see if many complain about this
			// version.
			//
			// FIXME: Displays the scaled memory usage instead
			// of the original.
			if (opt->dict_size < (UINT32_C(1) << 20))
				memlimit_too_small(memory_usage, memory_limit);

			memory_usage = lzma_raw_encoder_memusage(filters);
			if (memory_usage == UINT64_MAX)
				message_bug();

			// Accept it if it is low enough.
			if (memory_usage <= memory_limit)
				break;

			// Otherwise 1 MiB down and try again. I hope this
			// isn't too slow method for cases where the original
			// dict_size is very big.
			opt->dict_size -= UINT32_C(1) << 20;
		}

		// Tell the user that we decreased the dictionary size.
		// However, omit the message if no preset or custom chain
		// was given. FIXME: Always warn?
		if (!preset_default)
			message(V_WARNING, "Adjusted LZMA%c dictionary size "
					"from %s MiB to %s MiB to not exceed "
					"the memory usage limit of %s MiB",
					filters[i].id == LZMA_FILTER_LZMA2
						? '2' : '1',
					uint64_to_str(orig_dict_size >> 20, 0),
					uint64_to_str(opt->dict_size >> 20, 1),
					uint64_to_str(memory_limit >> 20, 2));
	}

/*
	// Limit the number of worker threads so that memory usage
	// limit isn't exceeded.
	assert(memory_usage > 0);
	size_t thread_limit = memory_limit / memory_usage;
	if (thread_limit == 0)
		thread_limit = 1;

	if (opt_threads > thread_limit)
		opt_threads = thread_limit;
*/

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
					hardware_memlimit_get(), flags);
			break;

		case FORMAT_XZ:
			ret = lzma_stream_decoder(&strm,
					hardware_memlimit_get(), flags);
			break;

		case FORMAT_LZMA:
			ret = lzma_alone_decoder(&strm,
					hardware_memlimit_get());
			break;

		case FORMAT_RAW:
			// Memory usage has already been checked in
			// coder_set_compression_settings().
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
coder_main(file_pair *pair)
{
	// Buffers to hold input and output data.
	uint8_t in_buf[IO_BUFFER_SIZE];
	uint8_t out_buf[IO_BUFFER_SIZE];

	// Initialize the progress indicator.
	const uint64_t in_size = pair->src_st.st_size <= (off_t)(0)
			? 0 : (uint64_t)(pair->src_st.st_size);
	message_progress_start(&strm, pair->src_name, in_size);

	lzma_action action = LZMA_RUN;
	lzma_ret ret;
	bool success = false; // Assume that something goes wrong.

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
				break;

			strm.next_out = out_buf;
			strm.avail_out = IO_BUFFER_SIZE;
		}

		if (ret != LZMA_OK) {
			// Determine if the return value indicates that we
			// won't continue coding.
			const bool stop = ret != LZMA_NO_CHECK
					&& ret != LZMA_UNSUPPORTED_CHECK;

			if (stop) {
				// Write the remaining bytes even if something
				// went wrong, because that way the user gets
				// as much data as possible, which can be good
				// when trying to get at least some useful
				// data out of damaged files.
				if (opt_mode != MODE_TEST && io_write(pair,
						out_buf, IO_BUFFER_SIZE
							- strm.avail_out))
					break;
			}

			if (ret == LZMA_STREAM_END) {
				// Check that there is no trailing garbage.
				// This is needed for LZMA_Alone and raw
				// streams.
				if (strm.avail_in == 0 && !pair->src_eof) {
					// Try reading one more byte.
					// Hopefully we don't get any more
					// input, and thus pair->src_eof
					// becomes true.
					strm.avail_in = io_read(
							pair, in_buf, 1);
					if (strm.avail_in == SIZE_MAX)
						break;

					assert(strm.avail_in == 0
							|| strm.avail_in == 1);
				}

				if (strm.avail_in == 0) {
					assert(pair->src_eof);
					success = true;
					break;
				}

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
				// Figure out how much memory it would have
				// actually needed.
				uint64_t memusage = lzma_memusage(&strm);
				uint64_t memlimit = hardware_memlimit_get();

				// Round the memory limit down and usage up.
				// This way we don't display a ridiculous
				// message like "Limit was 9 MiB, but 9 MiB
				// would have been needed".
				memusage = (memusage + 1024 * 1024 - 1)
						/ (1024 * 1024);
				memlimit /= 1024 * 1024;

				message_error(_("Limit was %s MiB, "
						"but %s MiB would "
						"have been needed"),
						uint64_to_str(memlimit, 0),
						uint64_to_str(memusage, 1));
			}

			if (stop)
				break;
		}

		// Show progress information under certain conditions.
		message_progress_update();
	}

	message_progress_end(success);

	return success;
}


extern void
coder_run(const char *filename)
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
	const bool success = coder_main(pair);

	// Close the file pair. It needs to know if coding was successful to
	// know if the source or target file should be unlinked.
	io_close(pair, success);

	return;
}
