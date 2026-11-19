cmake_minimum_required(VERSION 3.26)
project(generator LANGUAGES CXX)
include(FetchContent)
# Force shared library build for trajoptlib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build using shared libraries" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
if(MSVC)
    message(STATUS "Building with MSVC")
endif()

FetchContent_Declare(
    TCLAP
    EXCLUDE_FROM_ALL
    GIT_REPOSITORY "https://github.com/mirror/tclap"
)
FetchContent_MakeAvailable(TCLAP)

if(NOT TARGET TrajoptLib)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../trajoptlib"
        "${CMAKE_CURRENT_BINARY_DIR}/trajoptlib"
    )
endif()

include(../cmake/CompilerFlags.cmake)

if(NOT TARGET choreo::document_core)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../document/cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/choreo_document_core"
    )
endif()

# Collect sources for the executable
file(
    GLOB_RECURSE generator_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_parser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/split_to_segments.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/progress_update_sender/*.cpp"
)

add_executable(generator ${generator_SOURCES})

suppress_clangcl_unused_command_line_argument(generator)

target_compile_definitions(generator PRIVATE CHOREO_WITH_TRAJOPT)

target_include_directories(
    generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${tclap_SOURCE_DIR}/include
)

target_link_libraries(
    generator
    PRIVATE
        TrajoptLib
        choreo::document_core
        wpinet
        wpiutil
)

install(TARGETS generator RUNTIME DESTINATION bin)
