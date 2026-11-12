cmake_minimum_required(VERSION 3.21)
project(wpilib_expression_sample LANGUAGES CXX)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(WPILIB_WITH_CSCORE OFF CACHE INTERNAL "Build CSCore")
set(WPILIB_WITH_GUI OFF CACHE INTERNAL "Build GUI")
set(WPILIB_WITH_NTCORE ON CACHE INTERNAL "Build NTCore")
set(WPILIB_WITH_SIMULATION_MODULES OFF CACHE INTERNAL "Build simulation modules")
set(WPILIB_WITH_TESTS OFF CACHE INTERNAL "Build WPILib tests")
set(WPILIB_WITH_WPIMATH ON CACHE INTERNAL "Build WPIMath")
set(WPILIB_WITH_WPILIB OFF CACHE INTERNAL "Build WPILib")
set(WPILIB_NO_WERROR ON CACHE INTERNAL "Disable WPILib warnings as errors")

set(WPILIB_USE_SYSTEM_EIGEN ON CACHE BOOL "Use fetched Eigen")
set(EIGEN_BUILD_CMAKE_PACKAGE TRUE)
FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://github.com/eigen-mirror/eigen.git
    GIT_TAG b7f6aed1b9974fd56cce651b4ff059c3bfc67782
    SYSTEM
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(Eigen3)

set(WPILIB_USE_SYSTEM_SLEIPNIR OFF CACHE INTERNAL "Use bundled Sleipnir")
FetchContent_Declare(
    wpilib
    GIT_REPOSITORY https://github.com/wpilibsuite/allwpilib.git
    GIT_TAG f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a
    EXCLUDE_FROM_ALL
    SYSTEM
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(wpilib)

if(TARGET wpimath AND NOT TARGET wpimath::wpimath)
    add_library(wpimath::wpimath ALIAS wpimath)
endif()

if(TARGET wpiutil AND NOT TARGET wpiutil::wpiutil)
    add_library(wpiutil::wpiutil ALIAS wpiutil)
endif()

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