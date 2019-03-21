###############################################################################
# (c) Copyright 2018 CERN for the benefit of the LHCb Collaboration           #
#                                                                             #
# This software is distributed under the terms of the Apache version 2        #
# licence, copied verbatim in the file "COPYING".                             #
#                                                                             #
# In applying this licence, CERN does not waive the privileges and immunities #
# granted to it by virtue of its status as an Intergovernmental Organization  #
# or submit itself to any jurisdiction.                                       #
###############################################################################
cmake_minimum_required(VERSION 3.10)

# Project settings

project(GitCondDB VERSION 0.1.0)

execute_process(COMMAND git describe --tags --always --first-parent HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GitCondDB_GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)


option(BUILD_SHARED_LIBS "Build shared library" ON)
option(CMAKE_EXPORT_COMPILE_COMMANDS "" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_COVERAGE} -g -O0 --coverage")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(CMAKE_CXX_EXTENSIONS OFF)

# external dependencies

find_package(PkgConfig)
pkg_check_modules(git2 libgit2 REQUIRED IMPORTED_TARGET)

find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp)
if(NOT JSON_INCLUDE_DIR)
  file(DOWNLOAD https://github.com/nlohmann/json/releases/download/v3.2.0/json.hpp "${CMAKE_BINARY_DIR}/nlohmann/json.hpp"
    SHOW_PROGRESS STATUS JSON_HPP_DOWNLOAD_STATUS
    EXPECTED_HASH MD5=d7e8c50249635575d3271e41dda4e7c7)
  list(GET JSON_HPP_DOWNLOAD_STATUS 0 JSON_HPP_DOWNLOAD_STATUS)
  if(NOT JSON_HPP_DOWNLOAD_STATUS EQUAL 0)
    message(FATAL_ERROR "Could not download json.hpp")
  endif()
  set(JSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Path to nlohmann/json.hpp" FORCE)
endif()
include_directories("${JSON_INCLUDE_DIR}")

find_package(fmt 5.2 REQUIRED)


include(GenerateExportHeader)


# Build instructions

set(HEADERS include/GitCondDB.h)
set(SOURCES src/common.h src/git_helpers.h src/iov_helpers.h src/DBImpl.h src/BasicLogger.h src/GitCondDB.cpp)

add_library(GitCondDB ${HEADERS} ${SOURCES})
generate_export_header(GitCondDB)

# Generate a version header
configure_file(cmake/GitCondDBVersion.h.in GitCondDBVersion.h)

target_include_directories(GitCondDB PRIVATE include)
target_link_libraries(GitCondDB PRIVATE PkgConfig::git2 fmt::fmt)
target_link_libraries(GitCondDB PUBLIC stdc++fs)

set_property(TARGET GitCondDB PROPERTY VERSION ${GitCondDB_VERSION})
set_property(TARGET GitCondDB PROPERTY SOVERSION 1)
set_property(TARGET GitCondDB PROPERTY
  INTERFACE_GitCondDB_MAJOR_VERSION 1)
set_property(TARGET GitCondDB APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING GitCondDB_MAJOR_VERSION
)
set_property(TARGET GitCondDB PROPERTY
  PUBLIC_HEADER ${HEADERS})
set_property(TARGET GitCondDB APPEND PROPERTY
  PUBLIC_HEADER "${CMAKE_CURRENT_BINARY_DIR}/gitconddb_export.h" "${CMAKE_CURRENT_BINARY_DIR}/GitCondDBVersion.h")

target_compile_features(GitCondDB PUBLIC cxx_std_17)

set_property(TARGET GitCondDB PROPERTY COMPILE_DEFINITIONS_RELEASE -DNDEBUG)
set_property(TARGET GitCondDB PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO -DNDEBUG)
set_property(TARGET GitCondDB PROPERTY COMPILE_DEFINITIONS_MINSIZEREL -DNDEBUG)


# installation

install(TARGETS GitCondDB EXPORT GitCondDBTargets
    LIBRARY DESTINATION lib
      COMPONENT Runtime
      #NAMELINK_COMPONENT Devel
    ARCHIVE DESTINATION lib
      COMPONENT Devel
    RUNTIME DESTINATION bin
      COMPONENT Runtime
    PUBLIC_HEADER
      DESTINATION include
      COMPONENT Devel
    INCLUDES DESTINATION include)


# package exports

include(CMakePackageConfigHelpers)

set(ConfigPackageLocation lib/cmake/GitCondDB)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/GitCondDB/GitCondDBConfigVersion.cmake"
  VERSION ${GitCondDB_VERSION}
  COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(cmake/GitCondDBConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/GitCondDB/GitCondDBConfig.cmake"
  INSTALL_DESTINATION ${ConfigPackageLocation}
)

export(EXPORT GitCondDBTargets
    NAMESPACE GitCondDB::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/GitCondDB/GitCondDBTargets.cmake")

install(EXPORT GitCondDBTargets
  NAMESPACE GitCondDB::
    DESTINATION ${ConfigPackageLocation})

install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/GitCondDB/GitCondDBConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/GitCondDB/GitCondDBConfigVersion.cmake"
    DESTINATION
      ${ConfigPackageLocation}
    COMPONENT
      Devel
)


# testing
enable_testing()

find_package(GTest QUIET)
if(NOT GTEST_FOUND)
  include(ExternalProject)

  # check if we have system wide sources
  if(IS_DIRECTORY /usr/src/googletest)
    set(GTEST_SOURCES SOURCE_DIR /usr/src/googletest)
  else()
    set(GTEST_SOURCES
      URL https://github.com/google/googletest/archive/release-1.8.1.tar.gz
      URL_HASH SHA256=9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c)
  endif()

  ExternalProject_Add(googletest-distribution
    ${GTEST_SOURCES}
    CMAKE_CACHE_ARGS -DCMAKE_INSTALL_PREFIX:SRTING=<INSTALL_DIR>
                     -DBUILD_GTEST:BOOL=YES -DBUILD_GMOCK:BOOL=NO
                     -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=YES
                     -DCMAKE_CXX_STANDARD:STRING=17 -DCMAKE_CXX_EXTENSIONS:BOOL=OFF)

  ExternalProject_Get_Property(googletest-distribution INSTALL_DIR)

  file(MAKE_DIRECTORY "${INSTALL_DIR}/include")

  add_library(GTest::GTest STATIC IMPORTED)
  set_target_properties(GTest::GTest PROPERTIES
    IMPORTED_LOCATION "${INSTALL_DIR}/lib/libgtest.a"
    INTERFACE_INCLUDE_DIRECTORIES "${INSTALL_DIR}/include")
  find_package(Threads QUIET)
  if(TARGET Threads::Threads)
    set_target_properties(GTest::GTest PROPERTIES
      INTERFACE_LINK_LIBRARIES "Threads::Threads")
  endif()

  add_library(GTest::Main STATIC IMPORTED)
  set_target_properties(GTest::Main PROPERTIES
    IMPORTED_LOCATION "${INSTALL_DIR}/lib/libgtest_main.a"
    INTERFACE_INCLUDE_DIRECTORIES "${INSTALL_DIR}/include")

  add_custom_command(OUTPUT "${INSTALL_DIR}/lib/libgtest.a" "${INSTALL_DIR}/lib/libgtest_main.a"
    DEPENDS googletest-distribution)
  add_custom_target(googletest-local-install DEPENDS "${INSTALL_DIR}/lib/libgtest.a" "${INSTALL_DIR}/lib/libgtest_main.a")
endif()


# - prepare test repositories
if(NOT CMAKE_VERSION VERSION_LESS 3.12)
  find_package(Python REQUIRED COMPONENTS Interpreter)
else()
  find_package(PythonInterp REQUIRED)
  add_executable(Python::Interpreter IMPORTED)
  set_property(TARGET Python::Interpreter PROPERTY IMPORTED_LOCATION "${PYTHON_EXECUTABLE}")
endif()

add_custom_command(
  COMMENT "Generating test data"
  OUTPUT ${CMAKE_BINARY_DIR}/test_data/.stamp
  COMMAND Python::Interpreter ${CMAKE_SOURCE_DIR}/tests/prepare_test_data.py
  COMMAND ${CMAKE_COMMAND} -E touch test_data/.stamp
  DEPENDS tests/prepare_test_data.py)

add_custom_target(TestData DEPENDS ${CMAKE_BINARY_DIR}/test_data/.stamp)

# - unit test executables
include(GoogleTest)

foreach(subsystem CondDB  FS  Git  Helpers  JSON)
  add_executable(test_${subsystem} src/tests/test_common.h src/tests/${subsystem}_UnitTests.cpp)
  target_include_directories(test_${subsystem} PRIVATE include src)
  target_link_libraries(test_${subsystem} GitCondDB PkgConfig::git2 fmt::fmt GTest::GTest GTest::Main)
  if(TARGET googletest-distribution)
    add_dependencies(test_${subsystem} googletest-distribution)
  endif()
  gtest_discover_tests(test_${subsystem})
  add_dependencies(test_${subsystem} TestData)
endforeach()


# - coverage reports
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  find_program(lcov_COMMAND NAMES lcov)
  find_program(genhtml_COMMAND NAMES genhtml)
  if(lcov_COMMAND AND genhtml_COMMAND)
    add_custom_target(clean-coverage-stats
      COMMAND ${lcov_COMMAND} --zerocounters --directory .
      COMMENT "Cleaning coverage statistics")
    add_custom_target(generate-coverage-report
      COMMAND ${lcov_COMMAND} --quiet --capture --directory . --output-file coverage.info
      COMMAND ${lcov_COMMAND} --quiet --output-file coverage.info --remove coverage.info '${CMAKE_BINARY_DIR}/googletest-distribution-prefix/*' '/usr/*' '*/json.hpp' '*/BasicLogger.h' '${CMAKE_SOURCE_DIR}/src/tests/*'
      COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_report
      COMMAND ${genhtml_COMMAND} coverage.info --output-directory coverage_report
      COMMENT "Generate coverage report")
    add_custom_target(coverage-report
      COMMAND ${CMAKE_COMMAND} --build . --target clean-coverage-stats
      COMMAND ${CMAKE_COMMAND} --build . --target test || true
      COMMAND ${CMAKE_COMMAND} --build . --target generate-coverage-report)
  endif()
endif()
