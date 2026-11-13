file(WRITE "${TEST_DIR}/repl-test-input.txt" ":reload\n2 m + offset\ntarget.Rotation().Degrees()\ntarget.Rotation().Degrees() / deg\n:quit\n")
execute_process(
    COMMAND "${SAMPLE}" --variables "${VARIABLES}"
    INPUT_FILE "${TEST_DIR}/repl-test-input.txt"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0 OR NOT output MATCHES "Variables reloaded\\." OR NOT output MATCHES "2\\.9144 m" OR
    NOT output MATCHES "1\\.570[0-9]+ deg" OR NOT output MATCHES "90" OR NOT error STREQUAL "")
    message(FATAL_ERROR "Variables REPL failed (${result}): ${output} ${error}")
endif()

file(WRITE "${TEST_DIR}/repl-test-input.txt" ":reload\n1 m\n:quit\n")
execute_process(
    COMMAND "${SAMPLE}"
    INPUT_FILE "${TEST_DIR}/repl-test-input.txt"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0 OR NOT output MATCHES "1 m" OR NOT error MATCHES "No variables file specified")
    message(FATAL_ERROR "REPL without variables failed (${result}): ${output} ${error}")
endif()

execute_process(
    COMMAND "${SAMPLE}" --variables "${TEST_DIR}/missing-variables.txt"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(result EQUAL 0 OR NOT error MATCHES "Could not open variables file")
    message(FATAL_ERROR "Missing variables file was not reported: ${error}")
endif()

file(WRITE "${TEST_DIR}/repl-test-invalid.txt" "offset = eval(\"1\")\n")
execute_process(
    COMMAND "${SAMPLE}" --variables "${TEST_DIR}/repl-test-invalid.txt"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(result EQUAL 0 OR NOT error MATCHES "Invalid variables file")
    message(FATAL_ERROR "Invalid variables file was not reported: ${error}")
endif()

execute_process(COMMAND "${SAMPLE}" --variables RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Usage:")
    message(FATAL_ERROR "Missing variables argument was not reported: ${error}")
endif()

execute_process(COMMAND "${SAMPLE}" --unknown RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Usage:")
    message(FATAL_ERROR "Unknown argument was not reported: ${error}")
endif()
