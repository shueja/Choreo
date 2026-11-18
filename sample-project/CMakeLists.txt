cmake_minimum_required(VERSION 3.21)
project(wpilib_expression_sample LANGUAGES CXX)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/FetchWPILib.cmake")
fetch_wpilib()

find_package(wpilib-expression CONFIG REQUIRED)

add_executable(expression_sample main.cpp)
target_compile_features(expression_sample PRIVATE cxx_std_23)
target_link_libraries(expression_sample PRIVATE wpilib::expression)

enable_testing()
add_test(NAME expression_sample_smoke COMMAND expression_sample --smoke)
add_test(
    NAME expression_sample_repl
    COMMAND ${CMAKE_COMMAND}
        -DSAMPLE=$<TARGET_FILE:expression_sample>
        -DVARIABLES=${CMAKE_CURRENT_SOURCE_DIR}/example-variables.txt
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/repl-test.cmake
)