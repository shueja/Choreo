file(WRITE "${TEST_DIR}/repl-test-input.txt" ":reload\n2 m + offset\ntarget.Rotation().Degrees()\ntarget.Rotation().Degrees() / deg\n:quit\n")
execute_process(
    COMMAND "${SAMPLE}" --setup "${SETUP}"
    INPUT_FILE "${TEST_DIR}/repl-test-input.txt"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0 OR NOT output MATCHES "Setup reloaded\\." OR NOT output MATCHES "2\\.9144 SI" OR
    NOT output MATCHES "1\\.5708 SI" OR NOT output MATCHES "90 SI" OR NOT error STREQUAL "")
    message(FATAL_ERROR "Setup REPL failed (${result}): ${output} ${error}")
endif()

file(WRITE "${TEST_DIR}/repl-test-input.txt" ":reload\n1 m\n:quit\n")
execute_process(
    COMMAND "${SAMPLE}"
    INPUT_FILE "${TEST_DIR}/repl-test-input.txt"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0 OR NOT output MATCHES "1 SI" OR NOT error MATCHES "No setup file specified")
    message(FATAL_ERROR "REPL without setup failed (${result}): ${output} ${error}")
endif()

execute_process(
    COMMAND "${SAMPLE}" --setup "${TEST_DIR}/missing-setup-file.chai"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(result EQUAL 0 OR NOT error MATCHES "Could not open setup file")
    message(FATAL_ERROR "Missing setup file was not reported: ${error}")
endif()

file(WRITE "${TEST_DIR}/repl-test-invalid.chai" "var distance = eval(\"1\");\n")
execute_process(
    COMMAND "${SAMPLE}" --setup "${TEST_DIR}/repl-test-invalid.chai"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(result EQUAL 0 OR NOT error MATCHES "Invalid setup file")
    message(FATAL_ERROR "Invalid setup file was not reported: ${error}")
endif()

execute_process(COMMAND "${SAMPLE}" --setup RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Usage:")
    message(FATAL_ERROR "Missing setup argument was not reported: ${error}")
endif()

execute_process(COMMAND "${SAMPLE}" --unknown RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Usage:")
    message(FATAL_ERROR "Unknown argument was not reported: ${error}")
endif()