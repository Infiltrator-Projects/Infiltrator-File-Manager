# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" INFILTRATOR_FILE_MANAGER_VERSION)
string(STRIP "${INFILTRATOR_FILE_MANAGER_VERSION}" INFILTRATOR_FILE_MANAGER_VERSION)
project(InfiltratorFileManager VERSION "${INFILTRATOR_FILE_MANAGER_VERSION}" LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Git REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(INFILTRATR_COMMON_EXPECTED_VERSION "1.19.38")
set(INFILTRATR_COMMON_EXPECTED_COMMIT "04b5e219924ec0e65ef9d254c114fad4de0abd29")
set(INFILTRATR_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common")

if(NOT EXISTS "${INFILTRATR_COMMON_DIR}/VERSION")
    message(FATAL_ERROR
        "Infiltrator File Manager requires the pinned Common submodule. "
        "Clone with --recurse-submodules or run git submodule update --init --recursive.")
endif()

file(READ "${INFILTRATR_COMMON_DIR}/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION STREQUAL INFILTRATR_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Infiltrator File Manager requires Common ${INFILTRATR_COMMON_EXPECTED_VERSION}; "
        "found ${INFILTRATR_COMMON_VERSION}")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${INFILTRATR_COMMON_DIR}" rev-parse HEAD
        RESULT_VARIABLE INFILTRATR_COMMON_GIT_RESULT
        OUTPUT_VARIABLE INFILTRATR_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT INFILTRATR_COMMON_GIT_RESULT EQUAL 0 OR
       NOT INFILTRATR_COMMON_COMMIT STREQUAL INFILTRATR_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Infiltrator File Manager requires Common commit "
            "${INFILTRATR_COMMON_EXPECTED_COMMIT}; found ${INFILTRATR_COMMON_COMMIT}")
    endif()
endif()

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
add_subdirectory("${INFILTRATR_COMMON_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/infiltratr-common" EXCLUDE_FROM_ALL)

pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.10)

add_library(infiltrator-files-core STATIC
    src/core/location.cpp
    src/core/operation_engine.cpp)
target_include_directories(infiltrator-files-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
target_compile_features(infiltrator-files-core PUBLIC cxx_std_20)
target_link_libraries(infiltrator-files-core PUBLIC InfiltratrCommon::Common)

add_executable(infiltrator-file-manager
    src/main.cpp
    src/ui/create_folder_controller.cpp
    src/ui/file_manager_window.cpp
    src/ui/theme_controller.cpp)
target_include_directories(infiltrator-file-manager PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(infiltrator-file-manager PRIVATE
    infiltrator-files-core
    PkgConfig::GTK4)

foreach(target infiltrator-files-core infiltrator-file-manager)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    endif()
endforeach()

if(BUILD_TESTING)
    add_executable(location-model-test tests/location_model_test.cpp)
    target_link_libraries(location-model-test PRIVATE infiltrator-files-core)
    add_test(NAME location-model COMMAND location-model-test)

    add_executable(operation-engine-test tests/operation_engine_test.cpp)
    target_link_libraries(operation-engine-test PRIVATE infiltrator-files-core)
    add_test(NAME operation-engine COMMAND operation-engine-test)
endif()

install(TARGETS infiltrator-file-manager
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES data/infiltrator-file-manager.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

set(CPACK_PACKAGE_NAME "infiltrator-file-manager")
set(CPACK_PACKAGE_VENDOR "Infiltrator Projects")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "InfiltratorOS file manager")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Infiltrator Projects")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)
