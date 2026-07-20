// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       index.c
/// \brief      Handling of .xz Indexes and some other Stream information
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "index.h"
#include "stream_flags_common.h"


/// \brief      Maximum number of Streams in lzma_index
///
/// Having a limit makes it simpler to prevent integer overflows.
/// index_cat_realloc_streams() assumes that this is a power of two
/// and less than UINT32_MAX.
#define STREAMS_MAX (UINT32_C(1) << 30)


/// \brief      For how many Records to allocate memory initially
#define RECORDS_DEFAULT 128


/// \brief      How many Records can be allocated per Stream
#define RECORDS_MAX (SIZE_MAX / 2 / sizeof(index_record))


typedef struct {
	lzma_vli uncompressed_sum;
	lzma_vli unpadded_sum;
} index_record;


typedef struct {
	/// Uncompressed start offset of this Stream.
	/// Subtract lzma_index.uncompressed_bias to get the uncompressed
	/// start offset relative to the beginning of the file.
	///
	/// This is uint64_t, not lzma_vli, because valid values
	/// can exceed LZMA_VLI_MAX.
	uint64_t uncompressed_base;

	/// Compressed start offset of this Stream
	uint64_t compressed_base;

	/// Total number of Blocks before this Stream
	uint64_t block_number_base;

	/// Number of elements allocated for records[]
	size_t records_allocated;

	/// Number of Records in this Stream
	size_t records_count;

	/// Size of the List of Records field in this Stream. This is used
	/// together with records_count to calculate the size of the Index
	/// field and thus the total size of the Stream.
	lzma_vli index_list_size;

	/// Stream Flags of this Stream. This is meaningful only if
	/// the Stream Flags have been told us with lzma_index_stream_flags().
	/// Initially stream_flags.version is set to UINT32_MAX to indicate
	/// that the Stream Flags are unknown.
	lzma_stream_flags stream_flags;

	/// Amount of Stream Padding after this Stream. This defaults to
	/// zero and can be set with lzma_index_stream_padding().
	lzma_vli stream_padding;

	/// Array of Records in this Stream
	index_record records[];

} index_stream;


struct lzma_index_s {
	/// Array of pointers to the Stream(s). Often there is just one Stream.
	index_stream **streams;

	/// Number of pointers allocated in "streams" above.
	size_t streams_allocated;

	/// Number of pointers in use in "streams" above.
	/// This can be at most STREAMS_MAX but let's use size_t still.
	size_t streams_count;

	/// streams[first_stream] points to the first index_stream in use.
	/// This way there can be preallocated space to insert index_streams
	/// before the currently-first index_stream.
	size_t first_stream;

	/// The start of the file is uncompressed_bias instead of being fixed
	/// at zero. This way an index_stream can be prepended to "streams"
	/// by updating the uncompressed_bias here and uncompressed_base
	/// in the newly-inserted index_stream; there's no need to update
	/// all index_streams in lzma_index when exactly one index_stream
	/// is added using lzma_index_cat() (the common use case).
	uint64_t uncompressed_bias;

	// The start of the file is compressed_bias instead of being fixed
	// at zero. See above.
	uint64_t compressed_bias;

	// The first Block in the file is block_number_bias instead of being
	// fixed at one. See above.
	uint64_t block_number_bias;

	/// Uncompressed size of all the Blocks in the Stream(s)
	lzma_vli uncompressed_size;

	/// Total size of all the Blocks in the Stream(s)
	lzma_vli total_size;

	/// Total number of Records in all Streams in this lzma_index
	lzma_vli records_count;

	/// Size of the List of Records field if all the Streams in this
	/// lzma_index were packed into a single Stream (makes it simpler to
	/// take many .xz files and combine them into a single Stream).
	///
	/// This value together with records_count is needed to calculate
	/// Backward Size that is stored into Stream Footer.
	lzma_vli index_list_size;

	/// Bitmask indicating what integrity check types have been used
	/// as set by lzma_index_stream_flags(). The bit of the last Stream
	/// is not included here, since it is possible to change it by
	/// calling lzma_index_stream_flags() again.
	uint32_t checks;
};


extern lzma_index *
lzma_index_init2(const lzma_allocator *allocator, lzma_vli records_prealloc)
{
	// Prevent an integer overflow.
	if (records_prealloc > RECORDS_MAX)
		return NULL;

	lzma_index *i = lzma_alloc(sizeof(lzma_index), allocator);
	if (i == NULL)
		return NULL;

	// Allocate memory for one Stream with space for records_count
	// Records. It's only in lzma_index_cat() where the number of
	// Streams per lzma_index can grow.
	index_stream *s = lzma_alloc(sizeof(index_stream)
		+ (size_t)records_prealloc * sizeof(index_record), allocator);
	i->streams = lzma_alloc(1 * sizeof(index_stream *), allocator);
	if (s == NULL || i->streams == NULL) {
		lzma_free(i->streams, allocator);
		lzma_free(s, allocator);
		lzma_free(i, allocator);
		return NULL;
	}

	i->streams[0] = s;
	i->streams_allocated = 1;
	i->streams_count = 1;
	i->first_stream = 0;

	// The LZMA_VLI_MAX == UINT64_MAX / 2 is good as initial bias
	// because maximum compressed and uncompressed sizes cannot exceed
	// LZMA_VLI_MAX. The maximum number of Blocks/Records is much
	// smaller because each Block consumes at least a few bytes of space,
	// so LZMA_VLI_MAX is fine for that too.
	i->uncompressed_bias = LZMA_VLI_MAX;
	i->compressed_bias = LZMA_VLI_MAX;
	i->block_number_bias = LZMA_VLI_MAX;

	i->uncompressed_size = 0;
	i->total_size = 0;
	i->records_count = 0;
	i->index_list_size = 0;
	i->checks = 0;

	s->uncompressed_base = i->uncompressed_bias;
	s->compressed_base = i->compressed_bias;
	s->block_number_base = i->block_number_bias;
	s->records_allocated = records_prealloc;
	s->records_count = 0;
	s->index_list_size = 0;
	s->stream_flags.version = UINT32_MAX;
	s->stream_padding = 0;

	return i;
}


extern LZMA_API(lzma_index *)
lzma_index_init(const lzma_allocator *allocator)
{
	return lzma_index_init2(allocator, RECORDS_DEFAULT);
}


extern LZMA_API(void)
lzma_index_end(lzma_index *i, const lzma_allocator *allocator)
{
	if (i != NULL) {
		for (size_t k = 0; k < i->streams_count; ++k)
			lzma_free(i->streams[i->first_stream + k], allocator);

		lzma_free(i->streams, allocator);
		lzma_free(i, allocator);
	}

	return;
}


extern LZMA_API(uint64_t)
lzma_index_memusage(lzma_vli streams, lzma_vli blocks)
{
	// This calculates an upper bound that is only a little bit
	// bigger than the exact maximum memory usage with the given
	// parameters. FIXME?

	// Prevent integer overflows.
	if (streams == 0 || streams > STREAMS_MAX
			|| blocks > UINT64_MAX / sizeof(index_record))
		return UINT64_MAX;

	// Typical malloc() overhead is 2 * sizeof(void *) but we count
	// a little bit extra just in case. Using LZMA_MEMUSAGE_BASE
	// instead would give too inaccurate estimate.
	const uint64_t alloc_overhead = 4 * sizeof(void *);

	// Memory used by the base structure + its overhead.
	const uint64_t base_mem = sizeof(lzma_index) + alloc_overhead;

	// Memory used by the lzma_index.streams[] (an array of pointers).
	// The pointers are allocated in power-of-two amounts by
	// lzma_index_cat() -> index_cat_realloc_streams(). That is,
	// we might have twice the amount of memory allocated for the
	// pointers than is currently needed.
	//
	// In file_info.c it's good if we never underestimate the memory
	// usage of two combined lzma_indexes, so multiply the numbers of
	// Streams by two when calculating the memory usage of the pointers.
	const uint64_t pointers_mem = 2 * streams * sizeof(index_stream *)
			+ alloc_overhead;

	// Memory used by the index_stream structures, including
	// the index_records structs in index_stream.records[].
	// index_decoder.c uses lzma_index_init2() to preallocate
	// the required amount of index_records, thus it's best to
	// not add any extra like was done with pointers_mem above.
	// The downside is that other (uncommon) use cases will see
	// a value that is smaller than the actual allocations:
	// the .records[] array is reallocated as powers of two,
	// so with a huge number of Records the real memory use
	// after lzma_index_append() might be about twice our estimate.
	// FIXME?
	const uint64_t streams_mem
			= streams * (sizeof(index_stream) + alloc_overhead)
			+ blocks * sizeof(index_record);

	// Check that base_mem + pointers_mem + streams_mem doesn't overflow.
	// base_mem is tiny and pointers_mem is much smaller than UINT64_MAX,
	// so the subtractions cannot overflow.
	if (UINT64_MAX - base_mem - pointers_mem < streams_mem)
		return UINT64_MAX;

	return base_mem + pointers_mem + streams_mem;
}


extern LZMA_API(uint64_t)
lzma_index_memused(const lzma_index *i)
{
/*
	// The last Stream may have more space allocated in s->records
	// than are actually in use. It can happen after lzma_index_append().
	// In the worst case, the preallocated amount will double the
	// amount of memory in use. In practice this shouldn't matter.
	// Note that index_decoder.c uses lzma_index_init2(), which
	// ensures that any lzma_index from lzma_index_decoder() will
	// use minimal amount of memory.
	//
	// FIXME? Count preallocated or not? Mismatch between _memusage
	// and _memused is weird especially when _memused is larger.
	const index_stream *s = i->streams[
			i->first_stream + i->streams_count - 1];
	const size_t preallocated_records_count
			= s->records_allocated - s->records_count;
	return lzma_index_memusage(i->streams_count,
			i->records_count + preallocated_records_count);
*/
	// FIXME?
	return lzma_index_memusage(i->streams_count, i->records_count);
}


extern LZMA_API(lzma_vli)
lzma_index_block_count(const lzma_index *i)
{
	return i->records_count;
}


extern LZMA_API(lzma_vli)
lzma_index_stream_count(const lzma_index *i)
{
	return i->streams_count;
}


extern LZMA_API(lzma_vli)
lzma_index_size(const lzma_index *i)
{
	return index_size(i->records_count, i->index_list_size);
}


extern LZMA_API(lzma_vli)
lzma_index_total_size(const lzma_index *i)
{
	return i->total_size;
}


extern LZMA_API(lzma_vli)
lzma_index_stream_size(const lzma_index *i)
{
	// Stream Header + Blocks + Index + Stream Footer
	return LZMA_STREAM_HEADER_SIZE + i->total_size
			+ index_size(i->records_count, i->index_list_size)
			+ LZMA_STREAM_HEADER_SIZE;
}


static lzma_vli
index_file_size(lzma_vli compressed_base, lzma_vli unpadded_sum,
		lzma_vli record_count, lzma_vli index_list_size,
		lzma_vli stream_padding)
{
	// Earlier Streams and Stream Paddings + Stream Header
	// + Blocks + Index + Stream Footer + Stream Padding
	//
	// This might go over LZMA_VLI_MAX due to too big unpadded_sum
	// when this function is used in lzma_index_append().
	lzma_vli file_size = compressed_base + 2 * LZMA_STREAM_HEADER_SIZE
			+ stream_padding + vli_ceil4(unpadded_sum);
	if (file_size > LZMA_VLI_MAX)
		return LZMA_VLI_UNKNOWN;

	// The same applies here.
	file_size += index_size(record_count, index_list_size);
	if (file_size > LZMA_VLI_MAX)
		return LZMA_VLI_UNKNOWN;

	return file_size;
}


extern LZMA_API(lzma_vli)
lzma_index_file_size(const lzma_index *i)
{
	const index_stream *s = i->streams[
			i->first_stream + i->streams_count - 1];
	return index_file_size(s->compressed_base - i->compressed_bias,
		s->records_count == 0 ? 0
			: s->records[s->records_count - 1].unpadded_sum,
		s->records_count, s->index_list_size,
		s->stream_padding);
}


extern LZMA_API(lzma_vli)
lzma_index_uncompressed_size(const lzma_index *i)
{
	return i->uncompressed_size;
}


extern LZMA_API(uint32_t)
lzma_index_checks(const lzma_index *i)
{
	uint32_t checks = i->checks;

	// Get the type of the Check of the last Stream too.
	const index_stream *s = i->streams[
			i->first_stream + i->streams_count - 1];
	if (s->stream_flags.version != UINT32_MAX)
		checks |= UINT32_C(1) << s->stream_flags.check;

	return checks;
}


extern uint32_t
lzma_index_padding_size(const lzma_index *i)
{
	return (LZMA_VLI_C(4) - index_size_unpadded(
			i->records_count, i->index_list_size)) & 3;
}


extern LZMA_API(lzma_ret)
lzma_index_stream_flags(lzma_index *i, const lzma_stream_flags *stream_flags)
{
	if (i == NULL || stream_flags == NULL)
		return LZMA_PROG_ERROR;

	// Validate the Stream Flags.
	return_if_error(lzma_stream_flags_compare(
			stream_flags, stream_flags));

	index_stream *s = i->streams[i->first_stream + i->streams_count - 1];
	s->stream_flags = *stream_flags;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_index_stream_padding(lzma_index *i, lzma_vli stream_padding)
{
	if (i == NULL || stream_padding > LZMA_VLI_MAX
			|| (stream_padding & 3) != 0)
		return LZMA_PROG_ERROR;

	index_stream *s = i->streams[i->first_stream + i->streams_count - 1];

	// Check that the new value won't make the file grow too big.
	const lzma_vli old_stream_padding = s->stream_padding;
	s->stream_padding = 0;
	if (lzma_index_file_size(i) + stream_padding > LZMA_VLI_MAX) {
		s->stream_padding = old_stream_padding;
		return LZMA_DATA_ERROR;
	}

	s->stream_padding = stream_padding;
	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_index_append(lzma_index *i, const lzma_allocator *allocator,
		lzma_vli unpadded_size, lzma_vli uncompressed_size)
{
	// Validate.
	if (i == NULL || unpadded_size < UNPADDED_SIZE_MIN
			|| unpadded_size > UNPADDED_SIZE_MAX
			|| uncompressed_size > LZMA_VLI_MAX)
		return LZMA_PROG_ERROR;

	index_stream *s = i->streams[i->first_stream + i->streams_count - 1];

	const lzma_vli compressed_base = s->records_count == 0 ? 0
		: vli_ceil4(s->records[s->records_count - 1].unpadded_sum);
	const lzma_vli uncompressed_base = s->records_count == 0 ? 0
			: s->records[s->records_count - 1].uncompressed_sum;
	const uint32_t index_list_size_add = lzma_vli_size(unpadded_size)
			+ lzma_vli_size(uncompressed_size);

	// Check that uncompressed size will not overflow.
	if (uncompressed_base + uncompressed_size > LZMA_VLI_MAX)
		return LZMA_DATA_ERROR;

	// Check that the new unpadded sum will not overflow. This is
	// checked again in index_file_size(), but the unpadded sum is
	// passed to vli_ceil4() which expects a valid lzma_vli value.
	if (compressed_base + unpadded_size > UNPADDED_SIZE_MAX)
		return LZMA_DATA_ERROR;

	// Check that the file size will stay within limits.
	if (index_file_size(s->compressed_base - i->compressed_bias,
			compressed_base + unpadded_size, s->records_count + 1,
			s->index_list_size + index_list_size_add,
			s->stream_padding) == LZMA_VLI_UNKNOWN)
		return LZMA_DATA_ERROR;

	// The size of the Index field must not exceed the maximum value
	// that can be stored in the Backward Size field.
	if (index_size(i->records_count + 1,
			i->index_list_size + index_list_size_add)
			> LZMA_BACKWARD_SIZE_MAX)
		return LZMA_DATA_ERROR;

	if (s->records_allocated == s->records_count) {
		// We need to allocate more memory.
		size_t alloc_count;
		if (s->records_allocated < RECORDS_DEFAULT) {
			alloc_count = RECORDS_DEFAULT;

		} else if (s->records_allocated >= RECORDS_MAX) {
			return LZMA_MEM_ERROR;

		} else if (s->records_allocated > RECORDS_MAX / 2) {
			alloc_count = RECORDS_MAX;

		} else {
			// Pick the first power of two that is larger than
			// the current count. This way the reallocation
			// steps stay predictable even when the old
			// count isn't a power of two due to
			// lzma_index_init2() or lzma_index_dup().
			//
			// NOTE: The the 2*16 shift is to avoid
			// undefined behavior when size_t is 32 bits.
			alloc_count = s->records_allocated;
			alloc_count |= alloc_count >> 1;
			alloc_count |= alloc_count >> 2;
			alloc_count |= alloc_count >> 4;
			alloc_count |= alloc_count >> 8;
			alloc_count |= alloc_count >> 16;
			alloc_count |= (alloc_count >> 16) >> 16;
			++alloc_count;
		}

		// The above check for RECORDS_MAX ensures that
		// there's no integer overflow here.
		const size_t new_size = sizeof(index_stream)
				+ sizeof(index_record) * alloc_count;
		const size_t old_size = sizeof(index_stream)
				+ sizeof(index_record) * s->records_allocated;
		s = lzma_realloc(s, new_size, old_size, allocator);
		if (s == NULL)
			return LZMA_MEM_ERROR;

		i->streams[i->first_stream + i->streams_count - 1] = s;
		s->records_allocated = alloc_count;
	}

	assert(s->records != NULL);
	assert(s->records_allocated > 0);
	assert(s->records_count < s->records_allocated);

	// Add the new Record.
	s->records[s->records_count].uncompressed_sum
			= uncompressed_base + uncompressed_size;
	s->records[s->records_count].unpadded_sum
			= compressed_base + unpadded_size;

	// Update the totals.
	++s->records_count;
	s->index_list_size += index_list_size_add;

	i->total_size += vli_ceil4(unpadded_size);
	i->uncompressed_size += uncompressed_size;
	++i->records_count;
	i->index_list_size += index_list_size_add;

	return LZMA_OK;
}


/// Increase the size of the i->stream array so that it has at least "count"
/// elements. Caller must ensure that "count" won't exceed STREAMS_MAX.
static lzma_ret
index_cat_realloc_streams(lzma_index *i, size_t count,
		const lzma_allocator *allocator)
{
	assert(count >= 2);
	assert(count <= STREAMS_MAX);

	// Do nothing if we already have enough elements allocated.
	if (count <= i->streams_allocated)
		return LZMA_OK;

	// Round up to a power of two. This way we won't do too many
	// reallocations if a huge number of Streams are concatenated, and
	// the allocation sizes are predictable (won't depend on the Stream
	// counts of the lzma_indexes being concatenated).
	--count;
	count |= count >> 1;
	count |= count >> 2;
	count |= count >> 4;
	count |= count >> 8;
	count |= count >> 16;
	++count;

	// STREAMS_MAX is a power of two, so the above won't make
	// count larger than that. Check it anyway.
	assert(count <= STREAMS_MAX);
	if (count > STREAMS_MAX)
		return LZMA_PROG_ERROR;

	// Prevent an integer overflow with 32-bit size_t.
	if (count > SIZE_MAX / sizeof(index_stream *))
		return LZMA_MEM_ERROR;

	index_stream **new_streams = lzma_realloc(i->streams,
			sizeof(index_stream *) * count,
			sizeof(index_stream *) * i->streams_allocated,
			allocator);
	if (new_streams == NULL)
		return LZMA_MEM_ERROR;

	i->streams = new_streams;
	i->streams_allocated = count;
	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_index_cat(lzma_index *restrict dest, lzma_index *restrict src,
		const lzma_allocator *allocator)
{
	if (dest == NULL || src == NULL)
		return LZMA_PROG_ERROR;

	const lzma_vli dest_file_size = lzma_index_file_size(dest);

	// Check that we don't exceed the file size limits.
	if (dest_file_size + lzma_index_file_size(src) > LZMA_VLI_MAX
			|| dest->uncompressed_size + src->uncompressed_size
				> LZMA_VLI_MAX)
		return LZMA_DATA_ERROR;

	// Check that the encoded size of the combined lzma_indexes stays
	// within limits. In theory, this should be done only if we know
	// that the user plans to actually combine the Streams and thus
	// construct a single Index (probably rare). However, exceeding
	// this limit is quite theoretical, so we do this check always
	// to simplify things elsewhere.
	{
		const lzma_vli dest_size = index_size_unpadded(
				dest->records_count, dest->index_list_size);
		const lzma_vli src_size = index_size_unpadded(
				src->records_count, src->index_list_size);
		if (vli_ceil4(dest_size + src_size) > LZMA_BACKWARD_SIZE_MAX)
			return LZMA_DATA_ERROR;
	}

	// Catch if the Stream count would become too crazy.
	// Both counts are at most STREAMS_MAX which is
	// small enough that the sum won't overflow.
	if (dest->streams_count + src->streams_count > STREAMS_MAX)
		return LZMA_DATA_ERROR;

	// Remember all check types from dest, including the check type of
	// last Stream which isn't in dest->checks.
	const uint32_t dest_checks = lzma_index_checks(dest);

	// Minimize memory usage of the index_record structs in
	// the last index_stream in dest.
	//
	// NOTE: This modifies dest. This should be fine even
	// if we failed later.
	//
	// NOTE: The if-condition is always false when concatenating
	// lzma_indexes produced by lzma_index_decoder() because it uses
	// lzma_index_init2() to allocate the required number of Records.
	{
		const size_t stream_idx
			= dest->first_stream + dest->streams_count - 1;
		index_stream *s = dest->streams[stream_idx];

		if (s->records_count < s->records_allocated) {
			const size_t new_size = sizeof(index_stream)
				+ sizeof(index_record) * s->records_count;
			const size_t old_size = sizeof(index_stream)
				+ sizeof(index_record) * s->records_allocated;

			s = lzma_realloc(s, new_size, old_size, allocator);
			if (s == NULL)
				return LZMA_MEM_ERROR;

			dest->streams[stream_idx] = s;
			s->records_allocated = s->records_count;
		}
	}

	if (dest->streams_count == 1) {
		// dest has exactly one Stream and src has one or many Streams.
		// This is the most common use case of lzma_index_cat(),
		// including the use from file_info_decode() in file_info.c.
		//
		// We prepend the one Stream from dest (dest->streams[0]) to
		// src->streams, and update the biases in src. This way
		// the other index_stream structs in src->streams don't need
		// to be modified, and this function stays fast even if src
		// has a huge number of Streams. Finally the members are
		// copied from src to dest.
		assert(dest->streams_allocated == 1);
		assert(dest->first_stream == 0);

		// Reallocate more memory for src->streams if needed.
		return_if_error(index_cat_realloc_streams(
				src, 1 + src->streams_count, allocator));

		// The one pointer from dest will be inserted
		// before the existing pointers in src->stream.
		// If there is no room before the first array element,
		// move the pointers to the end of the array.
		//
		// (This is a bit dumb because lzma_realloc() might need to
		// memcpy(), and then we memmove() the same data here.)
		if (src->first_stream == 0) {
			src->first_stream = src->streams_allocated
					- src->streams_count;
			assert(src->first_stream > 0);
			memmove(src->streams + src->first_stream,
				src->streams,
				src->streams_count * sizeof(index_stream *));
		}

		dest->uncompressed_bias = src->uncompressed_bias
				- dest->uncompressed_size;
		dest->compressed_bias = src->compressed_bias - dest_file_size;
		dest->block_number_bias = src->block_number_bias
				- dest->records_count;

		dest->streams[0]->uncompressed_base = dest->uncompressed_bias;
		dest->streams[0]->compressed_base = dest->compressed_bias;
		dest->streams[0]->block_number_base = dest->block_number_bias;

		++src->streams_count;
		--src->first_stream;
		src->streams[src->first_stream] = dest->streams[0];

		lzma_free(dest->streams, allocator);
		dest->streams = src->streams;
		src->streams = NULL;
		dest->streams_allocated = src->streams_allocated;
		dest->streams_count = src->streams_count;
		dest->first_stream = src->first_stream;

	} else {
		// In the less common use cases, apppend the Streams from
		// src to dest, updating the _base members in every
		// index_stream that are moved from src.
		//
		// Reallocate more memory for dest->streams if needed.
		return_if_error(index_cat_realloc_streams(dest,
				dest->streams_count + src->streams_count,
				allocator));

		// Move the pointers to the beginning of the array if they
		// aren't there already.
		if (dest->first_stream > 0) {
			memmove(dest->streams,
				dest->streams + dest->first_stream,
				dest->streams_count * sizeof(index_stream *));
			dest->first_stream = 0;
		}

		// Append the index_streams from src to dest.
		// Don't update dest->streams_count yet because
		// we need the old value still.
		memcpy(dest->streams + dest->streams_count,
				src->streams + src->first_stream,
				src->streams_count * sizeof(index_stream *));
		lzma_free(src->streams, allocator);
		src->streams = NULL;

		// Fix index_stream.*_base in the index_streams that were
		// copied from src.
		//
		// NOTE: Unsigned integer wrap around might occur
		// here and again in the for-loop below.
		const uint64_t uncompressed_add = dest->uncompressed_bias
				- src->uncompressed_bias
				+ dest->uncompressed_size;
		const uint64_t compressed_add = dest->compressed_bias
				- src->compressed_bias
				+ dest_file_size;
		const uint64_t block_number_add = dest->block_number_bias
				- src->block_number_bias
				+ dest->records_count;

		for (size_t k = 0; k < src->streams_count; ++k) {
			dest->streams[dest->streams_count + k]
				->uncompressed_base += uncompressed_add;
			dest->streams[dest->streams_count + k]
				->compressed_base += compressed_add;
			dest->streams[dest->streams_count + k]
				->block_number_base += block_number_add;
		}

		dest->streams_count += src->streams_count;
	}

	// Update info about all the combined Streams.
	dest->uncompressed_size += src->uncompressed_size;
	dest->total_size += src->total_size;
	dest->records_count += src->records_count;
	dest->index_list_size += src->index_list_size;
	dest->checks = dest_checks | src->checks;

	// There's nothing else left in src than the base structure.
	lzma_free(src, allocator);

	return LZMA_OK;
}


extern LZMA_API(lzma_index *)
lzma_index_dup(const lzma_index *src, const lzma_allocator *allocator)
{
	// Allocate the base structure.
	lzma_index *dest = lzma_alloc(sizeof(lzma_index), allocator);
	if (dest == NULL)
		return NULL;

	// Copy the base structure. The pointer dest->streams, the size
	// dest->streams_allocated, and the array offset dest->first_stream
	// will be wrong.
	*dest = *src;

	// We won't allocate spare capacity. Because dest->first_stream is 0,
	// we don't need to specify it when indexing dest->stream[] later.
	dest->streams_allocated = dest->streams_count;
	dest->first_stream = 0;

	// Allocate the pointers to index_streams.
	dest->streams = lzma_alloc(
			dest->streams_allocated * sizeof(index_stream *),
			allocator);
	if (dest->streams == NULL) {
		lzma_free(dest, allocator);
		return NULL;
	}

	// Allocate and copy the index_stream structures along with
	// their index_record arrays. We don't allocate any spare capacity,
	// so dest->records_allocated = src->records_count.
	for (size_t k = 0; k < dest->streams_count; ++k) {
		const size_t records_count
			= src->streams[src->first_stream + k]->records_count;
		const size_t alloc_size = sizeof(index_stream)
				+ records_count * sizeof(index_record);

		dest->streams[k] = lzma_alloc(alloc_size, allocator);
		if (dest->streams[k] == NULL) {
			// Memory allocation failed. Free the
			// already-allocated structures.
			while (k > 0)
				lzma_free(dest->streams[--k], allocator);

			lzma_free(dest->streams, allocator);
			lzma_free(dest, allocator);
			return NULL;
		}

		// Copy the index_stream structure.
		// dest->streams[k]->records_allocated will be wrong.
		memcpy(dest->streams[k], src->streams[src->first_stream + k],
				alloc_size);
		dest->streams[k]->records_allocated = records_count;
	}

	return dest;
}


/// Indexing for lzma_index_iter.internal[]
enum {
	ITER_INDEX,
	ITER_STARTED,
	ITER_STREAM,
	ITER_RECORD,
};


static void
iter_set_info(lzma_index_iter *iter)
{
	const lzma_index *i = iter->internal[ITER_INDEX].p;
	const index_stream *stream = i->streams[
			i->first_stream + iter->internal[ITER_STREAM].s];
	const size_t record = iter->internal[ITER_RECORD].s;

	iter->stream.number = iter->internal[ITER_STREAM].s + 1;
	iter->stream.block_count = stream->records_count;
	iter->stream.compressed_offset
			= stream->compressed_base - i->compressed_bias;
	iter->stream.uncompressed_offset
			= stream->uncompressed_base - i->uncompressed_bias;

	// iter->stream.flags will be NULL if the Stream Flags haven't been
	// set with lzma_index_stream_flags().
	iter->stream.flags = stream->stream_flags.version == UINT32_MAX
			? NULL : &stream->stream_flags;
	iter->stream.padding = stream->stream_padding;

	if (stream->records_count == 0) {
		// Stream has no Blocks.
		iter->stream.compressed_size = index_size(0, 0)
				+ 2 * LZMA_STREAM_HEADER_SIZE;
		iter->stream.uncompressed_size = 0;
	} else {
		// Stream Header + Stream Footer + Index + Blocks
		iter->stream.compressed_size = 2 * LZMA_STREAM_HEADER_SIZE
			+ index_size(stream->records_count,
				stream->index_list_size)
			+ vli_ceil4(stream->records[
				stream->records_count - 1].unpadded_sum);
		iter->stream.uncompressed_size = stream->records[
				stream->records_count - 1].uncompressed_sum;

		iter->block.number_in_stream = record + 1;
		iter->block.number_in_file = iter->block.number_in_stream
				+ stream->block_number_base
				- i->block_number_bias;

		iter->block.compressed_stream_offset = record == 0 ? 0
			: vli_ceil4(stream->records[record - 1].unpadded_sum);
		iter->block.uncompressed_stream_offset = record == 0 ? 0
			: stream->records[record - 1].uncompressed_sum;

		iter->block.uncompressed_size
				= stream->records[record].uncompressed_sum
				- iter->block.uncompressed_stream_offset;
		iter->block.unpadded_size
				= stream->records[record].unpadded_sum
				- iter->block.compressed_stream_offset;
		iter->block.total_size = vli_ceil4(iter->block.unpadded_size);

		iter->block.compressed_stream_offset
				+= LZMA_STREAM_HEADER_SIZE;

		iter->block.compressed_file_offset
				= iter->block.compressed_stream_offset
				+ iter->stream.compressed_offset;
		iter->block.uncompressed_file_offset
				= iter->block.uncompressed_stream_offset
				+ iter->stream.uncompressed_offset;
	}

	return;
}


extern LZMA_API(void)
lzma_index_iter_init(lzma_index_iter *iter, const lzma_index *i)
{
	iter->internal[ITER_INDEX].p = i;
	lzma_index_iter_rewind(iter);
	return;
}


extern LZMA_API(void)
lzma_index_iter_rewind(lzma_index_iter *iter)
{
	iter->internal[ITER_STARTED].s = false;
	iter->internal[ITER_STREAM].s = 0;
	iter->internal[ITER_RECORD].s = 0;
	return;
}


extern LZMA_API(lzma_bool)
lzma_index_iter_next(lzma_index_iter *iter, lzma_index_iter_mode mode)
{
	// Catch unsupported mode values.
	if ((unsigned int)(mode) > LZMA_INDEX_ITER_NONEMPTY_BLOCK)
		return true;

	const lzma_index *i = iter->internal[ITER_INDEX].p;
	const bool is_started = iter->internal[ITER_STARTED].s;
	size_t stream_idx = iter->internal[ITER_STREAM].s;
	size_t record = iter->internal[ITER_RECORD].s;

again:
	if (!is_started) {
		// After _init() or _rewind(), start at the first Stream.
		stream_idx = 0;
		record = 0;

		if (mode >= LZMA_INDEX_ITER_BLOCK) {
			// We are being asked to return information about
			// the first Block. Skip Streams that have no Blocks.
			stream_idx = 0;
			while (i->streams[i->first_stream + stream_idx]
					->records_count == 0) {
				if (++stream_idx == i->streams_count)
					return true;
			}
		}

	} else if (mode != LZMA_INDEX_ITER_STREAM && record + 1 < i->streams[
			i->first_stream + stream_idx]->records_count) {
		// The next Record is in this Stream.
		++record;

	} else {
		// This Stream has no more Records or LZMA_INDEX_ITER_STREAM
		// was used. Find the next Stream.
		//
		// If we are being asked to return information about a Block,
		// we skip empty Streams.
		do {
			if (++stream_idx == i->streams_count)
				return true;
		} while (mode >= LZMA_INDEX_ITER_BLOCK
				&& i->streams[i->first_stream + stream_idx]
					->records_count == 0);

		record = 0;
	}

	if (mode == LZMA_INDEX_ITER_NONEMPTY_BLOCK) {
		// We need to look for the next Block again if this Block
		// is empty.
		const index_record *r = i->streams[
				i->first_stream + stream_idx]->records;
		if (record == 0) {
			if (r[0].uncompressed_sum == 0)
				goto again;
		} else if (r[record - 1].uncompressed_sum
				== r[record].uncompressed_sum) {
			goto again;
		}
	}

	iter->internal[ITER_STARTED].s = true;
	iter->internal[ITER_STREAM].s = stream_idx;
	iter->internal[ITER_RECORD].s = record;

	iter_set_info(iter);

	return false;
}


extern LZMA_API(lzma_bool)
lzma_index_iter_locate(lzma_index_iter *iter, lzma_vli target)
{
	const lzma_index *i = iter->internal[ITER_INDEX].p;

	// If the target is past the end of the file, return immediately.
	if (i->uncompressed_size <= target)
		return true;

	// The i->streams[...]->uncompressed_base values are biased.
	target += i->uncompressed_bias;

	// Use binary search to locate the Stream that contains the target
	// offset. It is the last Stream where uncompressed_base is still
	// less than or equal to the target offset.
	size_t stream_idx;
	{
		size_t left = i->first_stream;
		size_t right = i->first_stream + i->streams_count - 1;

		while (left < right) {
			const size_t k = left + (right - left + 1) / 2;
			if (i->streams[k]->uncompressed_base <= target)
				left = k;
			else
				right = k - 1;
		}

		stream_idx = left;
	}

	// Set target to the offset inside the Stream. This also removes
	// the bias that was added above.
	const index_stream *stream = i->streams[stream_idx];
	target -= stream->uncompressed_base;

	// Use binary search to locate the exact Record. It is the first
	// Record whose uncompressed_sum is greater than target.
	// This is because we want the rightmost Record that fulfills the
	// search criterion. It is possible that there are empty Blocks;
	// we don't want to return them.
	size_t record;
	{
		size_t left = 0;
		size_t right = stream->records_count - 1;

		while (left < right) {
			const size_t k = left + (right - left) / 2;
			if (stream->records[k].uncompressed_sum <= target)
				left = k + 1;
			else
				right = k;
		}

		record = left;
	}

	// The iterator must keep working if i->streams is reallocated
	// and the reserved space at the beginning changes, thus
	// i->first_stream is subtracted here.
	iter->internal[ITER_STARTED].s = true;
	iter->internal[ITER_STREAM].s = stream_idx - i->first_stream;
	iter->internal[ITER_RECORD].s = record;

	iter_set_info(iter);

	return false;
}
