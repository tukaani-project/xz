# SPDX-License-Identifier: 0BSD

#############################################################################
#
# Optional file to be included by the top-level CMakeLists.txt to run tests
#
# The CMake rules for the tests are in this separate optional file so
# that it's trivial to just delete the whole "tests" directory and still
# get an otherwise normal CMake-based build. This way it's easy to ensure
# that nothing in the "tests" directory can affect the build process.
#
# Author: Lasse Collin
#
#############################################################################

include(CTest)

if(BUILD_TESTING)
    set(LIBLZMA_TESTS
        test_bcj_exact_size
        test_block_header
        test_check
        test_filter_flags
        test_filter_str
        test_hardware
        test_index
        test_index_hash
        test_lzip_decoder
        test_memlimit
        test_stream_flags
        test_vli
    )

    # MicroLZMA encoder is needed for both encoder and decoder tests.
    # If MicroLZMA decoder is not configured but LZMA1 decoder is, then
    # test_microlzma will fail to compile because this configuration is
    # not possible in the Autotools build, so the test was not made to
    # support it since it would have required additional changes.
    if (MICROLZMA_ENCODER AND (MICROLZMA_DECODER
            OR NOT "lzma1" IN_LIST DECODERS))
        list(APPEND LIBLZMA_TESTS test_microlzma)
    endif()

    foreach(TEST IN LISTS LIBLZMA_TESTS)
        add_executable("${TEST}" "tests/${TEST}.c")

        target_include_directories("${TEST}" PRIVATE
            src/common
            src/liblzma/api
            src/liblzma
        )

        target_link_libraries("${TEST}" PRIVATE liblzma)

        # Put the test programs into their own subdirectory so they don't
        # pollute the top-level dir which might contain xz and xzdec.
        set_target_properties("${TEST}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests_bin"
        )

        add_test(NAME "${TEST}"
                 COMMAND "${CMAKE_CURRENT_BINARY_DIR}/tests_bin/${TEST}"
        )

        # Set srcdir environment variable so that the tests find their
        # input files from the source tree.
        #
        # Set the return code for skipped tests to match Automake convention.
        set_tests_properties("${TEST}" PROPERTIES
            ENVIRONMENT "srcdir=${CMAKE_CURRENT_SOURCE_DIR}/tests"
            SKIP_RETURN_CODE 77
        )
    endforeach()

    if(UNIX AND HAVE_DECODERS)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_scripts")

        add_test(NAME test_scripts.sh
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scripts.sh" ".."
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_scripts"
        )

        set_tests_properties(test_scripts.sh PROPERTIES
            ENVIRONMENT "srcdir=${CMAKE_CURRENT_SOURCE_DIR}/tests"
            SKIP_RETURN_CODE 77
        )
    endif()
endif()
