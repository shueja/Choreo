cmake_minimum_required(VERSION 3.21)
project(wpilib-expression VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
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
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
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

FetchContent_Declare(
    chaiscript
    GIT_REPOSITORY https://github.com/ChaiScript/ChaiScript.git
    GIT_TAG f635b51e3f8e95868210edfc66fdbd8e475710f5
    SOURCE_SUBDIR include
    SYSTEM
)
FetchContent_MakeAvailable(chaiscript)
set(CHAISCRIPT_INCLUDE_DIR "${chaiscript_SOURCE_DIR}/include")

FetchContent_Declare(
    llnl_units
    GIT_REPOSITORY https://github.com/LLNL/units.git
    GIT_TAG v0.13.1
    SOURCE_SUBDIR units
    SYSTEM
)
FetchContent_MakeAvailable(llnl_units)

if(TARGET wpimath AND NOT TARGET wpimath::wpimath)
    add_library(wpimath::wpimath ALIAS wpimath)
endif()

if(TARGET wpiutil AND NOT TARGET wpiutil::wpiutil)
    add_library(wpiutil::wpiutil ALIAS wpiutil)
endif()

set(CHAISCRIPT_COMPAT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/chaiscript-compat/include")
file(COPY "${CHAISCRIPT_INCLUDE_DIR}/chaiscript" DESTINATION "${CHAISCRIPT_COMPAT_INCLUDE_DIR}")
set(CHAISCRIPT_PROXY_FUNCTIONS "${CHAISCRIPT_COMPAT_INCLUDE_DIR}/chaiscript/dispatchkit/proxy_functions.hpp")
file(READ "${CHAISCRIPT_PROXY_FUNCTIONS}" CHAISCRIPT_PROXY_FUNCTIONS_CONTENT)
string(
    REPLACE
        "std::shared_ptr<AST_Node> t_parsenode = AST_NodePtr()"
        "std::shared_ptr<AST_Node> t_parsenode = nullptr"
        CHAISCRIPT_PROXY_FUNCTIONS_CONTENT
        "${CHAISCRIPT_PROXY_FUNCTIONS_CONTENT}"
)
file(WRITE "${CHAISCRIPT_PROXY_FUNCTIONS}" "${CHAISCRIPT_PROXY_FUNCTIONS_CONTENT}")

add_library(
    wpilib_expression
    src/expression.cpp
    src/expression_engine.cpp
    src/expression_syntax.cpp
    src/geometry_bindings.cpp
    src/quantity.cpp
    src/scalar_bindings.cpp
    src/units.cpp
)
add_library(wpilib::expression ALIAS wpilib_expression)
target_compile_features(wpilib_expression PUBLIC cxx_std_23)
target_include_directories(
    wpilib_expression
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CHAISCRIPT_COMPAT_INCLUDE_DIR}
)
if(MSVC AND DEFINED ENV{INCLUDE})
    target_include_directories(wpilib_expression SYSTEM PRIVATE $ENV{INCLUDE})
endif()
target_link_libraries(wpilib_expression PUBLIC wpimath::wpimath units)
set_target_properties(wpilib_expression PROPERTIES EXPORT_NAME expression)

option(WPILIB_EXPRESSION_BUILD_TESTS "Build expression library tests" ON)
if(WPILIB_EXPRESSION_BUILD_TESTS)
    enable_testing()
    add_executable(wpilib_expression_test tests/expression_test.cpp)
    target_link_libraries(wpilib_expression_test PRIVATE wpilib::expression)
    foreach(test_case IN ITEMS
        namespaces
        setup
        arithmetic_and_variables
        read_only_expressions
        error_messages
        context_copies
        unit_expressions
        identifiers
        measurement_math
        geometry_constructors
        geometry_operations
        shapes
    )
        add_test(NAME wpilib_expression.${test_case} COMMAND wpilib_expression_test ${test_case})
    endforeach()
endif()

install(
    TARGETS wpilib_expression
    EXPORT wpilib-expression-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT wpilib-expression-targets
    FILE wpilib-expression-targets.cmake
    NAMESPACE wpilib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-expression
)

configure_package_config_file(
    cmake/wpilib-expression-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wpilib-expression-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-expression
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wpilib-expression-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/wpilib-expression-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/wpilib-expression-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-expression
)