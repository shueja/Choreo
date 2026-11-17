cmake_minimum_required(VERSION 3.21)
project(wpilib-extras VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CTest)

if(NOT TARGET wpimath::wpimath)
    find_package(wpimath CONFIG QUIET)
endif()

if(NOT TARGET wpimath::wpimath)
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

    FetchContent_Declare(
        wpilib
        GIT_REPOSITORY https://github.com/wpilibsuite/allwpilib.git
        GIT_TAG f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a
        EXCLUDE_FROM_ALL
        SYSTEM
        UPDATE_DISCONNECTED TRUE
    )
    FetchContent_MakeAvailable(wpilib)
endif()

if(TARGET wpimath AND NOT TARGET wpimath::wpimath)
    add_library(wpimath::wpimath ALIAS wpimath)
endif()

add_library(wpilib_extras INTERFACE)
add_library(wpi::extras ALIAS wpilib_extras)
target_compile_features(wpilib_extras INTERFACE cxx_std_20)
target_include_directories(
    wpilib_extras
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(wpilib_extras INTERFACE wpimath::wpimath)
set_target_properties(wpilib_extras PROPERTIES EXPORT_NAME extras)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_executable(wpilib_extras_test tests/Polygon2dTest.cpp)
    target_link_libraries(wpilib_extras_test PRIVATE wpi::extras)
    add_test(NAME wpilib_extras.Polygon2d COMMAND wpilib_extras_test)
endif()

install(
    TARGETS wpilib_extras
    EXPORT wpilib-extras-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT wpilib-extras-targets
    FILE wpilib-extras-targets.cmake
    NAMESPACE wpi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-extras
)

configure_package_config_file(
    cmake/wpilib-extras-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wpilib-extras-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-extras
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wpilib-extras-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/wpilib-extras-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/wpilib-extras-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpilib-extras
)
