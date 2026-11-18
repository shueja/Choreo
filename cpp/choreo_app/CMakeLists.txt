cmake_minimum_required(VERSION 3.21)
project(choreo_app LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CHOREO_APP_USE_INSTALLED_DEPENDENCIES "Use installed document and expression packages" OFF)
if(CHOREO_APP_USE_INSTALLED_DEPENDENCIES)
    if(NOT TARGET choreo::document_core)
        find_package(choreo-document-core CONFIG REQUIRED)
    endif()
    if(NOT TARGET wpilib::expression)
        find_package(wpilib-expression CONFIG REQUIRED)
    endif()
else()
    if(NOT TARGET wpilib::expression)
        set(WPILIB_EXPRESSION_BUILD_TESTS OFF CACHE BOOL "Build expression tests")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../wpilib-expression" wpilib-expression)
    endif()
    if(NOT TARGET choreo::document_core)
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../document/cpp" choreo-document)
    endif()
endif()

FetchContent_Declare(
    immer
    GIT_REPOSITORY https://github.com/arximboldi/immer.git
    GIT_TAG 5875f7739a6c642ad58cbedadb509c86d421217e
    SOURCE_SUBDIR unused
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(immer)

add_library(choreo_document_app
    src/document_files.cpp
    src/document_store.cpp
    src/evaluate_variables.cpp
    src/rename_variable.cpp
)
add_library(choreo::document_app ALIAS choreo_document_app)
target_compile_features(choreo_document_app PUBLIC cxx_std_23)
target_include_directories(choreo_document_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${immer_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(choreo_document_app PUBLIC choreo::document_core wpilib::expression)
set_target_properties(choreo_document_app PROPERTIES EXPORT_NAME document_app)

if(TARGET wpinet)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../api/cpp" choreo-api)
    add_library(choreo_generation_process src/generation_process.cpp)
    add_library(choreo::generation_process ALIAS choreo_generation_process)
    target_compile_features(choreo_generation_process PUBLIC cxx_std_23)
    target_link_libraries(choreo_generation_process PUBLIC choreo::document_app PRIVATE wpinet)
endif()

install(TARGETS choreo_document_app EXPORT choreo-document-app-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT choreo-document-app
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT choreo-document-app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT choreo-document-app
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/choreo/document DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/choreo
    COMPONENT choreo-document-app
    PATTERN "generation_process.hpp" EXCLUDE
)
install(DIRECTORY "${immer_SOURCE_DIR}/immer" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT choreo-document-app
)
install(EXPORT choreo-document-app-targets FILE choreo-document-app-targets.cmake
    NAMESPACE choreo:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-app
    COMPONENT choreo-document-app
)
configure_package_config_file(cmake/choreo-document-app-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/choreo-document-app-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-app
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/choreo-document-app-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/choreo-document-app
    COMPONENT choreo-document-app
)

if(BUILD_TESTING)
    add_executable(choreo_document_core_test tests/document_test.cpp)
    target_link_libraries(choreo_document_core_test PRIVATE choreo::document_app)
    add_test(NAME choreo_document_core.snapshots COMMAND choreo_document_core_test "${CMAKE_CURRENT_BINARY_DIR}")
    if(TARGET choreo_generation_process)
        add_executable(choreo_generation_process_test tests/generation_process_test.cpp)
        target_link_libraries(choreo_generation_process_test PRIVATE choreo::generation_process)
        add_test(NAME choreo_generation_process.handoff COMMAND choreo_generation_process_test --handoff)
        foreach(case differential swerve split)
            add_test(NAME choreo_generation_process.real_${case} COMMAND choreo_generation_process_test --real-${case})
            set_tests_properties(choreo_generation_process.real_${case} PROPERTIES SKIP_RETURN_CODE 77)
        endforeach()
    endif()
endif()