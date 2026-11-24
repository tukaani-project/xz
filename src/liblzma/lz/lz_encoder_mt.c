// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder_mt.c
/// \brief      Threads for LZMA_MF_BT4MT
///
/// LZMA_MF_BT4MT runs LZMA_MF_BT4 as a pipeline: the hash thread updates
/// the hash tables, the binary tree thread updates the tree and queues
/// the matches, and the encoder thread combines them like
/// lzma_mf_bt4_find() does.
///
/// The threads read only positions that have nice_len +
/// LZMA_MEMCMPLEN_EXTRA bytes of input after them, so new input can be
/// copied into mf->buffer without locking.
//
//  Author:     Noelle
//
///////////////////////////////////////////////////////////////////////////////

#include "lz_encoder.h"
#include "memcmplen.h"


/// Number of positions in one block of the hash queue
#define HASH_BLOCK_SIZE (UINT32_C(1) << 12)

/// Number of blocks in the hash queue
#define HASH_BLOCK_COUNT 8

/// Size of one block in the match queue as lzma_match elements
#define MATCH_BLOCK_SIZE (UINT32_C(1) << 13)

/// Number of blocks in the match queue
#define MATCH_BLOCK_COUNT 16


/// A ring of blocks between a producer and a consumer thread
typedef struct {
	/// Number of blocks the producer may fill
	uint32_t free;

	/// Number of filled blocks the consumer hasn't taken yet
	uint32_t ready;

	/// Next block the producer will fill
	uint32_t write_index;

	/// Next block the consumer will take
	uint32_t read_index;
} queue;


struct lzma_mf_mt_s {
	/// The match finder that the threads work on
	lzma_mf *mf;

	/// The hash blocks, HASH_BLOCK_COUNT * HASH_BLOCK_SIZE * 3 elements
	uint32_t *hash_blocks;

	/// The match blocks, MATCH_BLOCK_COUNT * MATCH_BLOCK_SIZE elements
	lzma_match *match_blocks;

	mythread hash_thread;
	mythread bt_thread;

	/// Protects the members below except the encoder-only part
	mythread_mutex mutex;

	mythread_cond cond_hash;
	mythread_cond cond_bt;
	mythread_cond cond_encoder;

	/// Tells the threads to exit
	bool exit;

	/// Tells the threads not to start new work
	bool stop;

	/// True while the hash thread works without holding mutex
	bool hash_busy;

	/// True while the binary tree thread works without holding mutex
	bool bt_busy;

	/// The threads may process positions less than this. The positions
	/// are in the same units as mf->read_pos + mf->offset.
	uint32_t frontier;

	/// Next position that the hash thread will hash
	uint32_t hash_pos;

	/// Next position that the binary tree thread will insert
	uint32_t bt_pos;

	/// mf->cyclic_pos for bt_pos
	uint32_t bt_cyclic_pos;

	/// Hash thread to binary tree thread
	queue hash_queue;

	/// Number of positions in each hash block
	uint32_t hash_block_positions[HASH_BLOCK_COUNT];

	/// Binary tree thread to encoder thread
	queue match_queue;

	/// Number of positions in each match block
	uint32_t match_block_positions[MATCH_BLOCK_COUNT];

	/// Positions left in the hash block that the binary tree thread
	/// has taken. When this is zero, the thread holds no hash block.
	uint32_t bt_heads_left;

	/// Next unused entry in that hash block
	const uint32_t *bt_heads;

	/////////////////////////////////////
	// Used only by the encoder thread //
	/////////////////////////////////////

	/// True if the encoder thread has a match block taken
	bool holding;

	/// Next entry in the match block being read
	const lzma_match *read_ptr;

	/// Number of positions left in the match block being read
	uint32_t read_left;
};


static void
queue_reset(queue *q, uint32_t count)
{
	q->free = count;
	q->ready = 0;
	q->write_index = 0;
	q->read_index = 0;
	return;
}


extern uint64_t
lzma_mf_mt_memusage(void)
{
	return sizeof(lzma_mf_mt)
		+ (uint64_t)HASH_BLOCK_COUNT * HASH_BLOCK_SIZE * 3
			* sizeof(uint32_t)
		+ (uint64_t)MATCH_BLOCK_COUNT * MATCH_BLOCK_SIZE
			* sizeof(lzma_match);
}


/// Get the first position that the threads must not process with the
/// current input. The position of mf->write_pos can exceed UINT32_MAX
/// shortly before normalization, thus the 64-bit math.
static uint32_t
get_frontier(const lzma_mf *mf)
{
	const uint64_t need = (uint64_t)mf->nice_len + LZMA_MEMCMPLEN_EXTRA;
	const uint64_t end = (uint64_t)(uint32_t)(mf->read_pos + mf->offset)
			+ (mf->write_pos - mf->read_pos);
	if (end < need)
		return 0;

	uint64_t frontier = end - need + 1;

	// Position UINT32_MAX - 1 triggers normalization. The encoder
	// thread does it with the single-threaded code.
	if (frontier > UINT32_MAX - 1)
		frontier = UINT32_MAX - 1;

	return (uint32_t)frontier;
}


static MYTHREAD_RET_TYPE
hash_thread(void *thr_ptr)
{
	lzma_mf_mt *mt = thr_ptr;
	lzma_mf *mf = mt->mf;

	mythread_mutex_lock(&mt->mutex);

	while (!mt->exit) {
		if (mt->stop || mt->hash_queue.free == 0
				|| mt->hash_pos >= mt->frontier) {
			mythread_cond_wait(&mt->cond_hash, &mt->mutex);
			continue;
		}

		const uint32_t index = mt->hash_queue.write_index;
		mt->hash_queue.write_index
				= (index + 1) % HASH_BLOCK_COUNT;
		--mt->hash_queue.free;
		mt->hash_busy = true;

		const uint32_t pos = mt->hash_pos;
		const uint32_t count = my_min(mt->frontier - pos,
				HASH_BLOCK_SIZE);

		mythread_mutex_unlock(&mt->mutex);

		lzma_mf_bt4mt_hash(mf, pos, count, mt->hash_blocks
				+ (size_t)index * HASH_BLOCK_SIZE * 3);

		mythread_mutex_lock(&mt->mutex);

		mt->hash_block_positions[index] = count;
		++mt->hash_queue.ready;
		mt->hash_pos = pos + count;
		mt->hash_busy = false;

		mythread_cond_signal(&mt->cond_bt);
		mythread_cond_signal(&mt->cond_encoder);
	}

	mythread_mutex_unlock(&mt->mutex);

	return MYTHREAD_RET_VALUE;
}


static MYTHREAD_RET_TYPE
bt_thread(void *thr_ptr)
{
	lzma_mf_mt *mt = thr_ptr;
	lzma_mf *mf = mt->mf;

	mythread_mutex_lock(&mt->mutex);

	while (!mt->exit) {
		if (mt->stop || mt->match_queue.free == 0
				|| (mt->bt_heads_left == 0
					&& mt->hash_queue.ready == 0)) {
			mythread_cond_wait(&mt->cond_bt, &mt->mutex);
			continue;
		}

		if (mt->bt_heads_left == 0) {
			const uint32_t index = mt->hash_queue.read_index;
			mt->hash_queue.read_index
					= (index + 1) % HASH_BLOCK_COUNT;
			--mt->hash_queue.ready;

			mt->bt_heads = mt->hash_blocks
					+ (size_t)index * HASH_BLOCK_SIZE * 3;
			mt->bt_heads_left = mt->hash_block_positions[index];
		}

		const uint32_t index = mt->match_queue.write_index;
		mt->match_queue.write_index
				= (index + 1) % MATCH_BLOCK_COUNT;
		--mt->match_queue.free;
		mt->bt_busy = true;

		const uint32_t pos = mt->bt_pos;
		uint32_t cyclic_pos = mt->bt_cyclic_pos;
		const uint32_t count = mt->bt_heads_left;
		const uint32_t *heads = mt->bt_heads;

		mythread_mutex_unlock(&mt->mutex);

		const uint32_t done = lzma_mf_bt4mt_fill(mf, pos, &cyclic_pos,
				count, heads, mt->match_blocks
					+ (size_t)index * MATCH_BLOCK_SIZE,
				MATCH_BLOCK_SIZE);

		mythread_mutex_lock(&mt->mutex);

		assert(done > 0);
		mt->match_block_positions[index] = done;
		++mt->match_queue.ready;
		mt->bt_pos = pos + done;
		mt->bt_cyclic_pos = cyclic_pos;
		mt->bt_heads += 3 * (size_t)done;
		mt->bt_heads_left -= done;

		if (mt->bt_heads_left == 0) {
			++mt->hash_queue.free;
			mythread_cond_signal(&mt->cond_hash);
		}

		mt->bt_busy = false;
		mythread_cond_signal(&mt->cond_encoder);
	}

	mythread_mutex_unlock(&mt->mutex);

	return MYTHREAD_RET_VALUE;
}


/// True if the threads are idle and have nothing queued or to do.
/// mt->mutex must be locked.
static bool
is_idle(const lzma_mf_mt *mt)
{
	return !mt->hash_busy && !mt->bt_busy
			&& mt->hash_queue.ready == 0
			&& mt->bt_heads_left == 0
			&& mt->match_queue.ready == 0
			&& mt->hash_pos >= mt->frontier;
}


/// Wait until the threads aren't working and then discard everything
/// queued. The threads stay idle until lzma_mf_mt_init().
extern void
lzma_mf_mt_stop(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;

	mythread_sync(mt->mutex) {
		mt->stop = true;

		while (mt->hash_busy || mt->bt_busy)
			mythread_cond_wait(&mt->cond_encoder, &mt->mutex);

		queue_reset(&mt->hash_queue, HASH_BLOCK_COUNT);
		queue_reset(&mt->match_queue, MATCH_BLOCK_COUNT);
		mt->bt_heads_left = 0;
		mt->holding = false;
		mt->read_left = 0;
	}

	return;
}


extern void
lzma_mf_mt_end(lzma_mf *mf, const lzma_allocator *allocator)
{
	lzma_mf_mt *mt = mf->mt;
	if (mt == NULL)
		return;

	mythread_sync(mt->mutex) {
		mt->exit = true;
		mythread_cond_signal(&mt->cond_hash);
		mythread_cond_signal(&mt->cond_bt);
	}

	mythread_join(mt->hash_thread);
	mythread_join(mt->bt_thread);

	mythread_cond_destroy(&mt->cond_encoder);
	mythread_cond_destroy(&mt->cond_bt);
	mythread_cond_destroy(&mt->cond_hash);
	mythread_mutex_destroy(&mt->mutex);

	lzma_free(mt->match_blocks, allocator);
	lzma_free(mt->hash_blocks, allocator);
	lzma_free(mt, allocator);
	mf->mt = NULL;
	return;
}


static lzma_mf_mt *
create_mt(lzma_mf *mf, const lzma_allocator *allocator)
{
	lzma_mf_mt *mt = lzma_alloc(sizeof(lzma_mf_mt), allocator);
	if (mt == NULL)
		return NULL;

	mt->hash_blocks = lzma_alloc((size_t)HASH_BLOCK_COUNT
			* HASH_BLOCK_SIZE * 3 * sizeof(uint32_t), allocator);
	mt->match_blocks = lzma_alloc((size_t)MATCH_BLOCK_COUNT
			* MATCH_BLOCK_SIZE * sizeof(lzma_match), allocator);
	if (mt->hash_blocks == NULL || mt->match_blocks == NULL)
		goto error_blocks;

	if (mythread_mutex_init(&mt->mutex))
		goto error_blocks;

	if (mythread_cond_init(&mt->cond_hash))
		goto error_cond_hash;

	if (mythread_cond_init(&mt->cond_bt))
		goto error_cond_bt;

	if (mythread_cond_init(&mt->cond_encoder))
		goto error_cond_encoder;

	mt->mf = mf;
	mt->exit = false;
	mt->stop = false;
	mt->hash_busy = false;
	mt->bt_busy = false;
	mt->frontier = 0;
	queue_reset(&mt->hash_queue, HASH_BLOCK_COUNT);
	queue_reset(&mt->match_queue, MATCH_BLOCK_COUNT);
	mt->bt_heads_left = 0;
	mt->holding = false;
	mt->read_left = 0;

	if (mythread_create(&mt->hash_thread, &hash_thread, mt))
		goto error_hash_thread;

	if (mythread_create(&mt->bt_thread, &bt_thread, mt)) {
		mythread_sync(mt->mutex) {
			mt->exit = true;
			mythread_cond_signal(&mt->cond_hash);
		}

		mythread_join(mt->hash_thread);
		goto error_hash_thread;
	}

	return mt;

error_hash_thread:
	mythread_cond_destroy(&mt->cond_encoder);
error_cond_encoder:
	mythread_cond_destroy(&mt->cond_bt);
error_cond_bt:
	mythread_cond_destroy(&mt->cond_hash);
error_cond_hash:
	mythread_mutex_destroy(&mt->mutex);
error_blocks:
	lzma_free(mt->match_blocks, allocator);
	lzma_free(mt->hash_blocks, allocator);
	lzma_free(mt, allocator);
	return NULL;
}


/// Create the threads if needed and set their state from mf.
/// lzma_mf_mt_stop() must have been called before mf was reinitialized.
extern lzma_ret
lzma_mf_mt_init(lzma_mf *mf, const lzma_allocator *allocator)
{
	if (mf->mt == NULL) {
		mf->mt = create_mt(mf, allocator);
		if (mf->mt == NULL)
			return LZMA_MEM_ERROR;
	}

	lzma_mf_mt_resync(mf);
	return LZMA_OK;
}


/// Tell the threads about new input in mf->buffer.
extern void
lzma_mf_mt_update(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;
	const uint32_t frontier = get_frontier(mf);

	mythread_sync(mt->mutex) {
		if (frontier > mt->frontier) {
			mt->frontier = frontier;
			mythread_cond_signal(&mt->cond_hash);
		}
	}

	return;
}


/// Give the current position back to the threads after the encoder thread
/// has run the single-threaded match finder. mf->offset may have changed
/// due to normalization.
extern void
lzma_mf_mt_resync(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;

	mythread_sync(mt->mutex) {
		assert(!mt->hash_busy && !mt->bt_busy);
		assert(mt->hash_queue.ready == 0);
		assert(mt->match_queue.ready == 0);

		mt->stop = false;
		mt->bt_pos = mf->read_pos + mf->offset - mf->pending;
		mt->bt_cyclic_pos = mf->cyclic_pos;
		mt->hash_pos = mt->bt_pos;
		mt->frontier = get_frontier(mf);

		if (mt->frontier > mt->hash_pos)
			mythread_cond_signal(&mt->cond_hash);
	}

	return;
}


/// Get the queued result for the current position. If there is none
/// and the threads are idle, NULL is returned.
extern const lzma_match *
lzma_mf_mt_next(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;

	if (mt->read_left == 0) {
		mythread_mutex_lock(&mt->mutex);

		if (mt->holding) {
			mt->holding = false;
			++mt->match_queue.free;
			mythread_cond_signal(&mt->cond_bt);
		}

		while (mt->match_queue.ready == 0) {
			if (is_idle(mt)) {
				assert(mt->bt_pos == mf->read_pos
						+ mf->offset - mf->pending);
				assert(mt->bt_cyclic_pos == mf->cyclic_pos);
				mythread_mutex_unlock(&mt->mutex);
				return NULL;
			}

			mythread_cond_wait(&mt->cond_encoder, &mt->mutex);
		}

		const uint32_t index = mt->match_queue.read_index;
		mt->match_queue.read_index = (index + 1) % MATCH_BLOCK_COUNT;
		--mt->match_queue.ready;
		mt->holding = true;

		mythread_mutex_unlock(&mt->mutex);

		mt->read_ptr = mt->match_blocks
				+ (size_t)index * MATCH_BLOCK_SIZE;
		mt->read_left = mt->match_block_positions[index];
	}

	const lzma_match *entry = mt->read_ptr;
	mt->read_ptr += 2 + entry->len;
	--mt->read_left;

	return entry;
}


/// Wait until the threads aren't working and keep them idle until
/// lzma_mf_mt_resume(). This is needed while mf->buffer is moved.
extern void
lzma_mf_mt_pause(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;

	mythread_sync(mt->mutex) {
		mt->stop = true;

		while (mt->hash_busy || mt->bt_busy)
			mythread_cond_wait(&mt->cond_encoder, &mt->mutex);
	}

	return;
}


extern void
lzma_mf_mt_resume(lzma_mf *mf)
{
	lzma_mf_mt *mt = mf->mt;

	mythread_sync(mt->mutex) {
		mt->stop = false;
		mythread_cond_signal(&mt->cond_hash);
		mythread_cond_signal(&mt->cond_bt);
	}

	return;
}
