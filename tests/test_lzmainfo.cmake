# SPDX-License-Identifier: 0BSD

#############################################################################
#
# Check that lzmainfo reads binary headers identically from files and stdin.
#
#############################################################################

set(ENV{LC_ALL} C)

foreach(SIZE 26 2573)
    set(INPUT "${SRCDIR}/files/good-known_size-${SIZE}.lzma")
    set(HEADER "Uncompressed size:             0 MB (${SIZE} bytes)\n\
Dictionary size:               0 MB (2^18 bytes)\n\
Literal context bits (lc):     3\n\
Literal pos bits (lp):         0\n\
Number of pos bits (pb):       2\n")

    foreach(MODE file stdin dash)
        if(MODE STREQUAL "file")
            set(ARGUMENT "${INPUT}")
            set(EXPECTED "\n${INPUT}\n${HEADER}\n")
        elseif(MODE STREQUAL "dash")
            set(ARGUMENT "-")
            set(EXPECTED "\n${HEADER}")
        else()
            set(ARGUMENT)
            set(EXPECTED "${HEADER}")
        endif()

        execute_process(COMMAND "${LZMAINFO}" ${ARGUMENT}
            INPUT_FILE "${INPUT}"
            OUTPUT_VARIABLE OUTPUT
            ERROR_VARIABLE ERROR_OUTPUT
            RESULT_VARIABLE STATUS)
        string(REPLACE "\r\n" "\n" OUTPUT "${OUTPUT}")

        if(NOT STATUS STREQUAL "0" OR NOT OUTPUT STREQUAL EXPECTED)
            message(FATAL_ERROR
                "lzmainfo ${MODE} input with size ${SIZE} failed:\n"
                "status: ${STATUS}\n${ERROR_OUTPUT}${OUTPUT}")
        endif()
    endforeach()
endforeach()
