project(core VERSION 0.1.0 LANGUAGES CXX)

set(BUILD_WITH_EASY_PROFILER 1 CACHE STRING "BUILD_WITH_EASY_PROFILER" FORCE)
set(EASY_OPTION_LOG 1 CACHE STRING "EASY_OPTION_LOG" FORCE)

message(STATUS "Configuring ${PROJECT_NAME}...")

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)

configure_file("${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp.in" "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp" @ONLY)

if(${SBX_BUILD_SHARED})
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

add_library(${CMAKE_PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(easy_profiler CONFIG REQUIRED)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/core.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/engine.cpp"
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/application.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/cli.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/concepts.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/core.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/delegate.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/engine.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/exit.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/module.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/profiler.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/settings.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp"
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC
    # External dependencies
    fmt::fmt
    spdlog::spdlog
    range-v3::range-v3
    easy_profiler
    # stdc++_libbacktrace
    # Internal dependencies
    libsbx::utility
    libsbx::units
    libsbx::memory
)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(
  ${PROJECT_NAME}
  PUBLIC
    SBX_CONSTEXPR_ENABLED=$<IF:$<BOOL:${SBX_CONSTEXPR_ENABLED}>,1,0>
    BUILD_WITH_EASY_PROFILER
)

target_compile_features(
  ${PROJECT_NAME}
  PUBLIC
    cxx_std_23
)

target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
    -Wall 
    -Wextra
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
)

install(
  TARGETS
    ${PROJECT_NAME}
  EXPORT
    libsbxTargets
  LIBRARY 
    DESTINATION lib
  ARCHIVE 
    DESTINATION lib
  RUNTIME 
    DESTINATION bin
  FILE_SET 
    HEADERS 
)

set(_LINK_OPTIONS)
set(_COMPILE_OPTIONS)

if(${SBX_USE_ASAN} AND NOT MINGW)
  list(APPEND _COMPILE_OPTIONS -fsanitize=address,undefined -fno-omit-frame-pointer)
  list(APPEND _LINK_OPTIONS -fsanitize=address,undefined)
endif()

if(MINGW)
  list(APPEND _LINK_OPTIONS -Wl,--disable-dynamicbase,--default-image-base-low)
endif()

target_link_options(
  ${PROJECT_NAME}
  PUBLIC
    ${_LINK_OPTIONS}
)

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
    ${_COMPILE_OPTIONS}
)

if(${SBX_BUILD_TESTS} AND EXISTS "${PROJECT_SOURCE_DIR}/tests")
  add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()

if(${SBX_BUILD_BENCHMARKS} AND EXISTS "${PROJECT_SOURCE_DIR}/benchmarks")
  add_subdirectory(${PROJECT_SOURCE_DIR}/benchmarks)
endif()

