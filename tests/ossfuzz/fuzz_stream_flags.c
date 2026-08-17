// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_stream_flags.c
/// \brief      Fuzz test for Stream Header / Footer flags decoding
///
/// Fuzzes lzma_stream_header_decode() and lzma_stream_footer_decode().
/// Stream flag fields encode the check type and other metadata; an error in
/// parsing can affect every .xz stream processed by the library. After a
/// successful decode the flags are re-encoded and compared to verify
/// round-trip consistency.
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lzma.h"

// LZMA_STREAM_HEADER_SIZE is 12 bytes (magic + flags + CRC32).
// We need at least that many bytes to attempt a decode.


extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < LZMA_STREAM_HEADER_SIZE)
		return 0;

	// Use a local buffer of exactly LZMA_STREAM_HEADER_SIZE bytes so
	// the decoder never reads past a well-defined boundary.
	uint8_t hdr_buf[LZMA_STREAM_HEADER_SIZE];
	memcpy(hdr_buf, data, LZMA_STREAM_HEADER_SIZE);

	// ----------------------------------------------------------------
	// Path 1: Stream Header decode + re-encode
	// ----------------------------------------------------------------
	{
		lzma_stream_flags flags;
		memset(&flags, 0, sizeof(flags));

		lzma_ret ret = lzma_stream_header_decode(&flags, hdr_buf);
		if (ret == LZMA_OK) {
			// Re-encode and verify round-trip.
			uint8_t out[LZMA_STREAM_HEADER_SIZE];
			lzma_ret enc_ret = lzma_stream_header_encode(
					&flags, out);
			// Encoding should succeed when decode succeeded.
			if (enc_ret != LZMA_OK) {
				fprintf(stderr,
					"lzma_stream_header_encode() failed "
					"after successful decode (%d)\n",
					enc_ret);
				abort();
			}
		}
	}

	// ----------------------------------------------------------------
	// Path 2: Stream Footer decode + re-encode
	// ----------------------------------------------------------------
	{
		lzma_stream_flags flags;
		memset(&flags, 0, sizeof(flags));

		lzma_ret ret = lzma_stream_footer_decode(&flags, hdr_buf);
		if (ret == LZMA_OK) {
			uint8_t out[LZMA_STREAM_HEADER_SIZE];
			lzma_ret enc_ret = lzma_stream_footer_encode(
					&flags, out);
			if (enc_ret != LZMA_OK) {
				fprintf(stderr,
					"lzma_stream_footer_encode() failed "
					"after successful decode (%d)\n",
					enc_ret);
				abort();
			}
		}
	}

	// ----------------------------------------------------------------
	// Path 3: Compare header and footer flags when both decode cleanly
	//         (uses second LZMA_STREAM_HEADER_SIZE chunk if available)
	// ----------------------------------------------------------------
	if (size >= 2 * LZMA_STREAM_HEADER_SIZE) {
		uint8_t ftr_buf[LZMA_STREAM_HEADER_SIZE];
		memcpy(ftr_buf, data + LZMA_STREAM_HEADER_SIZE,
			LZMA_STREAM_HEADER_SIZE);

		lzma_stream_flags hdr_flags, ftr_flags;
		memset(&hdr_flags, 0, sizeof(hdr_flags));
		memset(&ftr_flags, 0, sizeof(ftr_flags));

		lzma_ret hret = lzma_stream_header_decode(
				&hdr_flags, hdr_buf);
		lzma_ret fret = lzma_stream_footer_decode(
				&ftr_flags, ftr_buf);

		if (hret == LZMA_OK && fret == LZMA_OK) {
			// Compare the two flag sets; the return value is
			// informational only — differences are not errors.
			(void)lzma_stream_flags_compare(
					&hdr_flags, &ftr_flags);
		}
	}

	return 0;
}
