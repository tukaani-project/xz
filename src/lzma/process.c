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


typedef struct {
	lzma_stream strm;
	void *options;

	file_pair *pair;

	/// We don't need this for *anything* but seems that at least with
	/// glibc pthread_create() doesn't allow NULL.
	pthread_t thread;

	bool in_use;

} thread_data;


/// Number of available threads
static size_t free_threads;

/// Thread-specific data
static thread_data *threads;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/// Attributes of new coder threads. They are created in detached state.
/// Coder threads signal to the service thread themselves when they are done.
static pthread_attr_t thread_attr;


//////////
// Init //
//////////

extern void
process_init(void)
{
	threads = malloc(sizeof(thread_data) * opt_threads);
	if (threads == NULL) {
		out_of_memory();
		my_exit(ERROR);
	}

	for (size_t i = 0; i < opt_threads; ++i)
		threads[i] = (thread_data){
			.strm = LZMA_STREAM_INIT_VAR,
			.options = NULL,
			.pair = NULL,
			.in_use = false,
		};

	if (pthread_attr_init(&thread_attr)
			|| pthread_attr_setdetachstate(
				&thread_attr, PTHREAD_CREATE_DETACHED)) {
		out_of_memory();
		my_exit(ERROR);
	}

	free_threads = opt_threads;

	return;
}


//////////////////////////
// Thread-specific data //
//////////////////////////

static thread_data *
get_thread_data(void)
{
	pthread_mutex_lock(&mutex);

	while (free_threads == 0) {
		pthread_cond_wait(&cond, &mutex);

		if (user_abort) {
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&mutex);
			return NULL;
		}
	}

	thread_data *t = threads;
	while (t->in_use)
		++t;

	t->in_use = true;
	--free_threads;

	pthread_mutex_unlock(&mutex);

	return t;
}


static void
release_thread_data(thread_data *t)
{
	pthread_mutex_lock(&mutex);

	t->in_use = false;
	++free_threads;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);

	return;
}


static int
create_thread(void *(*func)(thread_data *t), thread_data *t)
{
	if (opt_threads == 1) {
		func(t);
	} else {
		const int err = pthread_create(&t->thread, &thread_attr,
				(void *(*)(void *))(func), t);
		if (err) {
			errmsg(V_ERROR, _("Cannot create a thread: %s"),
					strerror(err));
			user_abort = 1;
			return -1;
		}
	}

	return 0;
}


/////////////////////////
// One thread per file //
/////////////////////////

static int
single_init(thread_data *t)
{
	lzma_ret ret;

	if (opt_mode == MODE_COMPRESS) {
		const lzma_vli uncompressed_size
				= t->pair->src_fd != STDIN_FILENO
				? (lzma_vli)(t->pair->src_st.st_size)
				: LZMA_VLI_VALUE_UNKNOWN;

		// TODO Support Multi-Block Streams to store Extra.
		if (opt_header == HEADER_ALONE) {
			lzma_options_alone alone;
			alone.uncompressed_size = uncompressed_size;
			memcpy(&alone.lzma, opt_filters[0].options,
					sizeof(alone.lzma));
			ret = lzma_alone_encoder(&t->strm, &alone);
		} else {
			lzma_options_stream stream = {
				.check = opt_check,
				.has_crc32 = true,
				.uncompressed_size = uncompressed_size,
				.alignment = 0,
			};
			memcpy(stream.filters, opt_filters,
					sizeof(stream.filters));
			ret = lzma_stream_encoder_single(&t->strm, &stream);
		}
	} else {
		// TODO Restrict file format if requested on the command line.
		ret = lzma_auto_decoder(&t->strm, NULL, NULL);
	}

	if (ret != LZMA_OK) {
		if (ret == LZMA_MEM_ERROR)
			out_of_memory();
		else
			internal_error();

		return -1;
	}

	return 0;
}


static lzma_ret
single_skip_padding(thread_data *t, uint8_t *in_buf)
{
	// Handle decoding of concatenated Streams. There can be arbitrary
	// number of nul-byte padding between the Streams, which must be
	// ignored.
	//
	// NOTE: Concatenating LZMA_Alone files works only if at least
	// one of lc, lp, and pb is non-zero. Using the concatenation
	// on LZMA_Alone files is strongly discouraged.
	while (true) {
		while (t->strm.avail_in > 0) {
			if (*t->strm.next_in != '\0')
				return LZMA_OK;

			++t->strm.next_in;
			--t->strm.avail_in;
		}

		if (t->pair->src_eof)
			return LZMA_STREAM_END;

		t->strm.next_in = in_buf;
		t->strm.avail_in = io_read(t->pair, in_buf, BUFSIZ);
		if (t->strm.avail_in == SIZE_MAX)
			return LZMA_DATA_ERROR;
	}
}


static void *
single(thread_data *t)
{
	if (single_init(t)) {
		io_close(t->pair, false);
		release_thread_data(t);
		return NULL;
	}

	uint8_t in_buf[BUFSIZ];
	uint8_t out_buf[BUFSIZ];
	lzma_action action = LZMA_RUN;
	lzma_ret ret;
	bool success = false;

	t->strm.avail_in = 0;

	while (!user_abort) {
		if (t->strm.avail_in == 0 && !t->pair->src_eof) {
			t->strm.next_in = in_buf;
			t->strm.avail_in = io_read(t->pair, in_buf, BUFSIZ);

			if (t->strm.avail_in == SIZE_MAX)
				break;
			else if (t->pair->src_eof
					&& opt_mode == MODE_COMPRESS)
				action = LZMA_FINISH;
		}

		t->strm.next_out = out_buf;
		t->strm.avail_out = BUFSIZ;

		ret = lzma_code(&t->strm, action);

		if (opt_mode != MODE_TEST)
			if (io_write(t->pair, out_buf,
					BUFSIZ - t->strm.avail_out))
				break;

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END) {
				if (opt_mode == MODE_COMPRESS) {
					success = true;
					break;
				}

				// Support decoding concatenated .lzma files.
				ret = single_skip_padding(t, in_buf);

				if (ret == LZMA_STREAM_END) {
					assert(t->pair->src_eof);
					success = true;
					break;
				}

				if (ret == LZMA_OK && !single_init(t))
					continue;

				break;

			} else {
				errmsg(V_ERROR, "%s: %s", t->pair->src_name,
						str_strm_error(ret));
				break;
			}
		}
	}

	io_close(t->pair, success);
	release_thread_data(t);

	return NULL;
}


///////////////////////////////
// Multiple threads per file //
///////////////////////////////

// TODO

// I'm not sure what would the best way to implement this. Here's one
// possible way:
//  - Reader thread would read the input data and control the coders threads.
//  - Every coder thread is associated with input and output buffer pools.
//    The input buffer pool is filled by reader thread, and the output buffer
//    pool is emptied by the writer thread.
//  - Writer thread writes the output data of the oldest living coder thread.
//
// The per-file thread started by the application's main thread is used as
// the reader thread. In the beginning, it starts the writer thread and the
// first coder thread. The coder thread would be left waiting for input from
// the reader thread, and the writer thread would be waiting for input from
// the coder thread.
//
// The reader thread reads the input data into a ring buffer, whose size
// depends on the value returned by lzma_chunk_size(). If the ring buffer
// gets full, the buffer is marked "to be finished", which indicates to
// the coder thread that no more input is coming. Then a new coder thread
// would be started.
//
// TODO

/*
typedef struct {
	/// Buffers
	uint8_t (*buffers)[BUFSIZ];

	/// Number of buffers
	size_t buffer_count;

	/// buffers[read_pos] is the buffer currently being read. Once finish
	/// is true and read_pos == write_pos, end of input has been reached.
	size_t read_pos;

	/// buffers[write_pos] is the buffer into which data is currently
	/// being written.
	size_t write_pos;

	/// This variable matters only when read_pos == write_pos && finish.
	/// In that case, this variable will contain the size of the
	/// buffers[read_pos].
	size_t last_size;

	/// True once no more data is being written to the buffer. When this
	/// is set, the last_size variable must have been set too.
	bool finish;

	/// Mutex to protect access to the variables in this structure
	pthread_mutex_t mutex;

	/// Condition to indicate when another thread can continue
	pthread_cond_t cond;
} mem_pool;


static foo
multi_reader(thread_data *t)
{
	bool done = false;

	do {
		const size_t size = io_read(t->pair,
				m->buffers + m->write_pos, BUFSIZ);
		if (size == SIZE_MAX) {
			// TODO
		} else if (t->pair->src_eof) {
			m->last_size = size;
		}

		pthread_mutex_lock(&m->mutex);

		if (++m->write_pos == m->buffer_count)
			m->write_pos = 0;

		if (m->write_pos == m->read_pos || t->pair->src_eof)
			m->finish = true;

		pthread_cond_signal(&m->cond);
		pthread_mutex_unlock(&m->mutex);

	} while (!m->finish);

	return done ? 0 : -1;
}


static foo
multi_code()
{
	lzma_action = LZMA_RUN;

	while (true) {
		pthread_mutex_lock(&m->mutex);

		while (m->read_pos == m->write_pos && !m->finish)
			pthread_cond_wait(&m->cond, &m->mutex);

		pthread_mutex_unlock(&m->mutex);

		if (m->finish) {
			t->strm.avail_in = m->last_size;
			if (opt_mode == MODE_COMPRESS)
				action = LZMA_FINISH;
		} else {
			t->strm.avail_in = BUFSIZ;
		}

		t->strm.next_in = m->buffers + m->read_pos;

		const lzma_ret ret = lzma_code(&t->strm, action);

	}
}

*/


///////////////////////
// Starting new file //
///////////////////////

extern void
process_file(const char *filename)
{
	thread_data *t = get_thread_data();
	if (t == NULL)
		return; // User abort

	// If this fails, it shows appropriate error messages too.
	t->pair = io_open(filename);
	if (t->pair == NULL) {
		release_thread_data(t);
		return;
	}

	// TODO Currently only one-thread-per-file mode is implemented.

	if (create_thread(&single, t)) {
		io_close(t->pair, false);
		release_thread_data(t);
	}

	return;
}
