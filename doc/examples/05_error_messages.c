/*
 * This example demonstrates how to retrieve detailed error messages
 * from lzma_code() when an error occurs.
 *
 * SPDX-License-Identifier: 0BSD
 */

#include <stdio.h>
#include <lzma.h>

int
main(void)
{
	printf("Example 1: Error message for programming error\n");
	printf("-----------------------------------------------\n\n");

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret;

	// Initialize a valid encoder
	ret = lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	if (ret != LZMA_OK) {
		fprintf(stderr, "Failed to initialize encoder\n");
		return 1;
	}

	// Try to call lzma_code with invalid arguments (NULL next_out with avail_out > 0)
	// This causes a LZMA_PROG_ERROR
	strm.next_out = NULL;
	strm.avail_out = 100;

	ret = lzma_code(&strm, LZMA_RUN);
	
	printf("Error code: %d\n", ret);
	const char *err_msg = lzma_get_error_message(&strm);
	if (err_msg != NULL) {
		printf("Error message: %s\n", err_msg);
	} else {
		printf("No error message available\n");
	}

	lzma_end(&strm);

	printf("\n\nExample 2: Error message for format error during decompression\n");
	printf("---------------------------------------------------------------\n\n");

	lzma_stream strm2 = LZMA_STREAM_INIT;
	
	// Initialize the decoder
	ret = lzma_stream_decoder(&strm2, UINT64_MAX, LZMA_CONCATENATED);
	if (ret != LZMA_OK) {
		fprintf(stderr, "Failed to initialize decoder\n");
		return 1;
	}

	// Try to decode clearly invalid/corrupted data
	uint8_t invalid_data[] = { 0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00,  // XZ header
	                           0xFF, 0xFF, 0xFF, 0xFF, 0xFF };       // Corrupted data
	strm2.next_in = invalid_data;
	strm2.avail_in = sizeof(invalid_data);
	
	uint8_t output_buffer[100];
	strm2.next_out = output_buffer;
	strm2.avail_out = sizeof(output_buffer);

	ret = lzma_code(&strm2, LZMA_RUN);
	
	printf("Result code: %d\n", ret);
	const char *err_msg2 = lzma_get_error_message(&strm2);
	if (err_msg2 != NULL) {
		printf("Error message: %s\n", err_msg2);
	} else {
		printf("No error message available\n");
	}

	lzma_end(&strm2);

	printf("\n\nExample 3: Error message is NULL after successful operation\n");
	printf("-----------------------------------------------------------\n\n");

	lzma_stream strm3 = LZMA_STREAM_INIT;
	
	// Initialize the encoder properly
	ret = lzma_easy_encoder(&strm3, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	if (ret != LZMA_OK) {
		fprintf(stderr, "Failed to initialize encoder\n");
		return 1;
	}

	// Set up input/output buffers
	uint8_t input_data[] = "Hello, World!";
	uint8_t output_buffer2[100];
	
	strm3.next_in = input_data;
	strm3.avail_in = sizeof(input_data) - 1;
	strm3.next_out = output_buffer2;
	strm3.avail_out = sizeof(output_buffer2);

	ret = lzma_code(&strm3, LZMA_FINISH);
	
	if (ret == LZMA_STREAM_END || ret == LZMA_OK) {
		printf("Encoding succeeded (return code: %d)\n", ret);
		const char *err_msg3 = lzma_get_error_message(&strm3);
		if (err_msg3 != NULL) {
			printf("Error message (should be NULL): %s\n", err_msg3);
		} else {
			printf("Error message: NULL (as expected)\n");
		}
	} else {
		printf("Encoding failed with error: %d\n", ret);
		const char *err_msg3 = lzma_get_error_message(&strm3);
		if (err_msg3 != NULL) {
			printf("Error message: %s\n", err_msg3);
		}
	}

	lzma_end(&strm3);

	return 0;
}
