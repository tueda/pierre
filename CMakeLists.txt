cmake_minimum_required(VERSION 3.15)

set(PROJECT_FULL_VERSION "0.1.1")

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" BASE_VERSION
             "${PROJECT_FULL_VERSION}")

option(ENABLE_VCPKG_FLINT "Enable FLINT dependency from vcpkg" ON)

if(ENABLE_VCPKG_FLINT)
  list(APPEND VCPKG_MANIFEST_FEATURES "flint")
endif()

project(
  pierre
  VERSION "${BASE_VERSION}"
  LANGUAGES CXX)

option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_SANITIZERS "Enable sanitizer instrumentation" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
option(ENABLE_STATIC_LINKING "Enable static linking for dependencies" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_BIN NAMES clang-tidy)
  if(NOT CLANG_TIDY_BIN)
    message(FATAL_ERROR "clang-tidy not found")
  endif()
endif()

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  add_custom_target(
    build-tests
    COMMENT "Building tests"
    VERBATIM)

  add_custom_target(
    check
    COMMAND "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" --output-on-failure
            --parallel
    DEPENDS build-tests
    COMMENT "Running tests"
    VERBATIM)

  if(ENABLE_CLANG_TIDY)
    add_subdirectory(tests)
  else()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
  endif()
endif()

if(ENABLE_COVERAGE)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "ENABLE_COVERAGE requires BUILD_TESTING to be enabled.")
  endif()
  find_program(GCOVR_EXECUTABLE gcovr)
  if(NOT GCOVR_EXECUTABLE)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
      message(WARNING "Neither gcovr nor lcov/genhtml was found.")
    endif()
  endif()
  if(GCOVR_EXECUTABLE)
    add_custom_target(
      coverage
      COMMAND "${CMAKE_COMMAND}" -P
              "${CMAKE_SOURCE_DIR}/cmake/reset-coverage-counters.cmake"
      COMMAND "${CMAKE_CTEST_COMMAND}" -C $<CONFIG> --output-on-failure
      COMMAND "${CMAKE_COMMAND}" -E make_directory
              "${CMAKE_BINARY_DIR}/coverage-report"
      COMMAND
        "${GCOVR_EXECUTABLE}" --root "${PROJECT_SOURCE_DIR}" --filter
        "${PROJECT_SOURCE_DIR}/src" --exclude-unreachable-branches
        --exclude-noncode-lines --exclude-throw-branches --html-details
        "${CMAKE_BINARY_DIR}/coverage-report/index.html" --xml
        "${CMAKE_BINARY_DIR}/coverage-report.xml" --print-summary
        "${CMAKE_BINARY_DIR}"
      DEPENDS build-tests
      COMMENT "Generating code coverage report with gcovr"
      VERBATIM)
  elseif(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    add_custom_target(
      coverage
      COMMAND "${LCOV_EXECUTABLE}" --zerocounters --directory
              "${CMAKE_BINARY_DIR}"
      COMMAND
        "${LCOV_EXECUTABLE}" --capture --initial --directory
        "${CMAKE_BINARY_DIR}" --base-directory "${PROJECT_SOURCE_DIR}"
        --output-file "${CMAKE_BINARY_DIR}/coverage.base.info" --ignore-errors
        inconsistent
      COMMAND "${CMAKE_CTEST_COMMAND}" -C $<CONFIG> --output-on-failure
      COMMAND
        "${LCOV_EXECUTABLE}" --capture --directory "${CMAKE_BINARY_DIR}"
        --base-directory "${PROJECT_SOURCE_DIR}" --output-file
        "${CMAKE_BINARY_DIR}/coverage.test.info" --ignore-errors inconsistent
      COMMAND
        "${LCOV_EXECUTABLE}" --add-tracefile
        "${CMAKE_BINARY_DIR}/coverage.base.info" --add-tracefile
        "${CMAKE_BINARY_DIR}/coverage.test.info" --output-file
        "${CMAKE_BINARY_DIR}/coverage.raw.info"
      COMMAND
        "${LCOV_EXECUTABLE}" --remove "${CMAKE_BINARY_DIR}/coverage.raw.info"
        "/usr/*" "*/vcpkg_installed/*" "*/tests/*" --output-file
        "${CMAKE_BINARY_DIR}/coverage.info" --ignore-errors unused,unused
      COMMAND "${LCOV_EXECUTABLE}" --summary "${CMAKE_BINARY_DIR}/coverage.info"
      COMMAND "${GENHTML_EXECUTABLE}" "${CMAKE_BINARY_DIR}/coverage.info"
              --output-directory "${CMAKE_BINARY_DIR}/coverage-report"
      DEPENDS build-tests
      COMMENT "Generating code coverage report with lcov"
      VERBATIM)
  else()
    add_custom_target(
      coverage
      COMMAND "${CMAKE_COMMAND}" -P
              "${CMAKE_SOURCE_DIR}/cmake/reset-coverage-counters.cmake"
      COMMAND "${CMAKE_CTEST_COMMAND}" -C $<CONFIG> --output-on-failure
      DEPENDS build-tests
      COMMENT "Running tests with coverage instrumentation"
      VERBATIM)
  endif()
endif()
