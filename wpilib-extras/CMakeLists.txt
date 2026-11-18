cmake_minimum_required(VERSION 3.21)
project(wpilib-extras VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CTest)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/FetchWPILib.cmake")
fetch_wpilib()

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
