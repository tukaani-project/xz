///////////////////////////////////////////////////////////////////////////////
//
/// \file       erofs_decoder.c
/// \brief      Decode EROFS LZMA format
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "lzma_decoder.h"
#include "lz_decoder.h"


typedef struct {
	/// LZMA1 decoder
	lzma_next_coder lzma;

	/// Uncompressed size of the stream as given by the application
	lzma_vli uncomp_size;

	/// LZMA dictionary size as given by the application
	uint32_t dict_size;

	/// True once the first byte of the EROFS LZMA stream
	/// has been processed.
	bool props_decoded;
} lzma_erofs_coder;


static lzma_ret
erofs_decode(void *coder_ptr, const lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	lzma_erofs_coder *coder = coder_ptr;

	if (!coder->props_decoded) {
		// There must be at least one byte of input to decode
		// the properties byte.
		if (*in_pos >= in_size)
			return LZMA_OK;

		lzma_options_lzma options = {
			.preset_dict = NULL,
			.preset_dict_size = 0,
		};

		// The properties are stored as bitwise-negation
		// of the typical encoding.
		if (lzma_lzma_lclppb_decode(&options, ~in[*in_pos]))
			return LZMA_OPTIONS_ERROR;

		++*in_pos;

		// Initialize the decoder.
		options.dict_size = coder->dict_size;
		lzma_filter_info filters[2] = {
			{
				.init = &lzma_lzma_decoder_init,
				.options = &options,
			}, {
				.init = NULL,
			}
		};

		return_if_error(lzma_next_filter_init(&coder->lzma,
				allocator, filters));

		// Use a hack to set the uncompressed size.
		lzma_lz_decoder_uncompressed(coder->lzma.coder,
				coder->uncomp_size);

		// Pass one dummy 0x00 byte to the LZMA decoder since that
		// is what it expects the first byte to be.
		const uint8_t dummy_in = 0;
		size_t dummy_in_pos = 0;
		if (coder->lzma.code(coder->lzma.coder, allocator,
				&dummy_in, &dummy_in_pos, 1,
				out, out_pos, out_size, LZMA_RUN) != LZMA_OK)
			return LZMA_PROG_ERROR;

		assert(dummy_in_pos == 1);
		coder->props_decoded = true;
	}

	// The rest is normal LZMA decoding.
	return coder->lzma.code(coder->lzma.coder, allocator,
				in, in_pos, in_size,
				out, out_pos, out_size, action);
}


static void
erofs_decoder_end(void *coder_ptr, const lzma_allocator *allocator)
{
	lzma_erofs_coder *coder = coder_ptr;
	lzma_next_end(&coder->lzma, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
erofs_decoder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		uint64_t uncomp_size, uint32_t dict_size)
{
	lzma_next_coder_init(&erofs_decoder_init, next, allocator);

	lzma_erofs_coder *coder = next->coder;

	if (coder == NULL) {
		coder = lzma_alloc(sizeof(lzma_erofs_coder), allocator);
		if (coder == NULL)
			return LZMA_MEM_ERROR;

		next->coder = coder;
		next->code = &erofs_decode;
		next->end = &erofs_decoder_end;

		coder->lzma = LZMA_NEXT_CODER_INIT;
	}

	if (uncomp_size > LZMA_VLI_MAX)
		return LZMA_OPTIONS_ERROR;

	coder->uncomp_size = uncomp_size;
	coder->dict_size = dict_size;

	coder->props_decoded = false;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_erofs_decoder(lzma_stream *strm, uint64_t uncomp_size, uint32_t dict_size)
{
	lzma_next_strm_init(erofs_decoder_init, strm, uncomp_size, dict_size);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}
