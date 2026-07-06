find_package(ctre CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLINT REQUIRED IMPORTED_TARGET flint)
find_package(quill CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)

add_library(${PROJECT_NAME}_impl cli.cpp env.cpp logger.cpp processor.cpp)
target_compile_features(${PROJECT_NAME}_impl PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(${PROJECT_NAME}_impl PRIVATE /templateDepth:2048)
endif()
target_include_directories(
  ${PROJECT_NAME}_impl
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(
  ${PROJECT_NAME}_impl
  PUBLIC PkgConfig::FLINT quill::quill
  PRIVATE ctre::ctre Microsoft.GSL::GSL)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_impl)

if(ENABLE_CLANG_TIDY)
  set_property(TARGET ${PROJECT_NAME}_impl PROPERTY CXX_CLANG_TIDY
                                                    "${CLANG_TIDY_BIN}")
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_CLANG_TIDY
                                               "${CLANG_TIDY_BIN}")
endif()

if(ENABLE_SANITIZERS)
  target_compile_options(
    ${PROJECT_NAME}_impl PUBLIC -fsanitize=address,undefined
                                -fno-omit-frame-pointer)
  target_link_options(${PROJECT_NAME}_impl PUBLIC -fsanitize=address,undefined)
endif()

if(ENABLE_COVERAGE)
  target_compile_options(${PROJECT_NAME}_impl PRIVATE --coverage)
  target_link_options(${PROJECT_NAME}_impl PUBLIC --coverage)
endif()

if(ENABLE_STATIC_LINKING)
  target_link_options(${PROJECT_NAME} PRIVATE -static)
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_SEARCH_START_STATIC ON
                                                   LINK_SEARCH_END_STATIC ON)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
