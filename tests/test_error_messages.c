/*
 * Test suite for lzma_get_error_message functionality
 *
 * SPDX-License-Identifier: 0BSD
 */

#include "tests.h"


static void
test_error_message_prog_error(void)
{
	// Test that LZMA_PROG_ERROR gets an error message
	lzma_stream strm = LZMA_STREAM_INIT;
	
	// Initialize an encoder
	lzma_ret ret = lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	assert_lzma_ret(ret, LZMA_OK);

	// Try to call lzma_code with invalid arguments (NULL next_out)
	strm.next_out = NULL;
	strm.avail_out = 100;  // Non-zero avail_out with NULL next_out causes PROG_ERROR
	
	ret = lzma_code(&strm, LZMA_RUN);
	assert_lzma_ret(ret, LZMA_PROG_ERROR);
	
	const char *err_msg = lzma_get_error_message(&strm);
	assert_true(err_msg != NULL);
	
	lzma_end(&strm);
}


static void
test_error_message_format_error(void)
{
	// Test that LZMA_FORMAT_ERROR gets an error message
	lzma_stream strm = LZMA_STREAM_INIT;
	
	// Initialize a decoder
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
	assert_lzma_ret(ret, LZMA_OK);

	// Try to decode data that doesn't have valid LZMA format
	uint8_t invalid_data[] = "Not LZMA data";
	strm.next_in = invalid_data;
	strm.avail_in = sizeof(invalid_data);
	
	uint8_t output_buffer[100];
	strm.next_out = output_buffer;
	strm.avail_out = sizeof(output_buffer);

	ret = lzma_code(&strm, LZMA_RUN);
	assert_lzma_ret(ret, LZMA_FORMAT_ERROR);
	
	const char *err_msg = lzma_get_error_message(&strm);
	assert_true(err_msg != NULL);
	
	lzma_end(&strm);
}


static void
test_error_message_none_on_success(void)
{
	// Test that error message is NULL after successful operation
	lzma_stream strm = LZMA_STREAM_INIT;
	
	// Initialize an encoder
	lzma_ret ret = lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	assert_lzma_ret(ret, LZMA_OK);

	// Set up valid input/output buffers
	uint8_t input_data[] = "Hello";
	uint8_t output_buffer[100];
	
	strm.next_in = input_data;
	strm.avail_in = sizeof(input_data) - 1;
	strm.next_out = output_buffer;
	strm.avail_out = sizeof(output_buffer);

	ret = lzma_code(&strm, LZMA_RUN);
	// This should return LZMA_OK
	assert_lzma_ret(ret, LZMA_OK);
	
	// Error message should be NULL for successful operations
	const char *err_msg = lzma_get_error_message(&strm);
	assert_true(err_msg == NULL);
	
	lzma_end(&strm);
}


static void
test_error_message_null_stream(void)
{
	// Test that lzma_get_error_message handles NULL stream
	const char *err_msg = lzma_get_error_message(NULL);
	assert_true(err_msg == NULL);
}


static void
test_error_message_cleared_after_end(void)
{
	// Test that error message is cleared after lzma_end
	lzma_stream strm = LZMA_STREAM_INIT;
	
	// Initialize an encoder
	lzma_ret ret = lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	assert_lzma_ret(ret, LZMA_OK);

	// Cause an error
	strm.next_out = NULL;
	strm.avail_out = 100;
	
	ret = lzma_code(&strm, LZMA_RUN);
	assert_lzma_ret(ret, LZMA_PROG_ERROR);
	
	// Error message should be set
	const char *err_msg = lzma_get_error_message(&strm);
	assert_true(err_msg != NULL);
	
	// After lzma_end, the error message should be cleared
	lzma_end(&strm);
	err_msg = lzma_get_error_message(&strm);
	assert_true(err_msg == NULL);
}


extern int
main(int argc, char *argv[])
{
	tuktest_start(argc, argv);
	tuktest_run(test_error_message_prog_error);
	tuktest_run(test_error_message_format_error);
	tuktest_run(test_error_message_none_on_success);
	tuktest_run(test_error_message_null_stream);
	tuktest_run(test_error_message_cleared_after_end);
	tuktest_end();

	return 0;
}
