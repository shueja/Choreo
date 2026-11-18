cmake_minimum_required(VERSION 3.21)
project(choreo_document_core LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT TARGET wpi::extras)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../wpilib-extras" wpilib-extras)
endif()

add_library(choreo_document_core
    src/codegen_config.cpp
    src/constraint.cpp
    src/differential_wheels.cpp
    src/event_marker.cpp
    src/event_marker_data.cpp
    src/heading_constraint.cpp
    src/keep_in_circle.cpp
    src/max_angular_velocity.cpp
    src/max_linear_velocity.cpp
    src/mecanum_wheels.cpp
    src/motor_config.cpp
    src/parameters.cpp
    src/project.cpp
    src/robot_config.cpp
    src/robot_trajectory.cpp
    src/swerve_wheels.cpp
    src/trajectory.cpp
    src/trajectory_output.cpp
    src/variable.cpp
    src/waypoint.cpp
    src/waypoint_id.cpp
    src/wheels.cpp
)
add_library(choreo::document_core ALIAS choreo_document_core)
target_compile_features(choreo_document_core PUBLIC cxx_std_23)
target_include_directories(choreo_document_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(choreo_document_core PUBLIC wpi::extras)
set_target_properties(choreo_document_core PROPERTIES EXPORT_NAME document_core)

install(TARGETS choreo_document_core EXPORT choreo-document-core-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT choreo-document-core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT choreo-document-core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT choreo-document-core
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/choreo/document DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/choreo
    COMPONENT choreo-document-core
)
install(EXPORT choreo-document-core-targets FILE choreo-document-core-targets.cmake
    NAMESPACE choreo:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-core
    COMPONENT choreo-document-core
)
configure_package_config_file(cmake/choreo-document-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/choreo-document-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-core
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/choreo-document-core-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-core
    COMPONENT choreo-document-core
)

if(BUILD_TESTING)
    add_executable(choreo_document_shared_test tests/shared_test.cpp)
    target_link_libraries(choreo_document_shared_test PRIVATE choreo::document_core)
    add_test(NAME choreo_document_core.shared COMMAND choreo_document_shared_test "${CMAKE_CURRENT_BINARY_DIR}")

    add_executable(choreolib_v4_test
        tests/choreolib_test.cpp
        ../../choreolib/src/main/native/cpp/choreo/Choreo.cpp
    )
    target_include_directories(choreolib_v4_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../choreolib/src/main/native/include"
    )
    target_link_libraries(choreolib_v4_test PRIVATE choreo::document_core)
    add_test(NAME choreolib.v4 COMMAND choreolib_v4_test
        "${CMAKE_CURRENT_BINARY_DIR}/choreolib-test-trajectory.traj")
endif()