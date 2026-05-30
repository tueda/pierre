add_custom_target(
  build-tests
  COMMENT "Building tests"
  VERBATIM)

add_custom_target(
  check
  COMMAND "${CMAKE_CTEST_COMMAND}" -C $<CONFIG> --output-on-failure
  DEPENDS build-tests
  COMMENT "Running tests"
  VERBATIM)

find_package(doctest CONFIG REQUIRED)

function(add_cpp_test name)
  add_executable("${name}" "${name}.cpp")
  target_link_libraries("${name}" PRIVATE doctest::doctest ${PROJECT_NAME}_impl)
  add_dependencies(build-tests "${name}")
  add_test(NAME "${name}" COMMAND "${name}")
  if(ENABLE_CLANG_TIDY)
    set_property(TARGET "${name}" PROPERTY CXX_CLANG_TIDY "${CLANG_TIDY_BIN}")
  endif()
endfunction()

add_cpp_test(test-processor)
