project(utility VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)

configure_file("${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp.in" "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp" @ONLY)

if(${SBX_BUILD_SHARED})
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

add_library(${CMAKE_PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(easy_profiler REQUIRED)
find_package(range-v3 REQUIRED)
find_package(lz4 REQUIRED)
find_package(magic_enum REQUIRED)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/compression.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/timer.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/utility.cpp"
  PUBLIC
    FILE_SET HEADERS
    FILES
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/algorithm.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/assert.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/bitmask.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/cast.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/compression.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/concepts.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/enable_private_constructor.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/enum.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/exception.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/expected.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/fast_mod.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/hash.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/hashed_string.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/iterator.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/layout.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/logger.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/make_array.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/multimap_key_range.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/noncopyable.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/overload.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/primitive.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/small_string.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/string_literal.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/target.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/timer.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/type_id.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/type_list.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/type_name.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/utility.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/zip.hpp"
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
    lz4::lz4
    magic_enum::magic_enum
    # Internal dependencies
    libsbx::units
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
    SBX_CONSTEXPR_ENABLED=${SBX_CONSTEXPR_ENABLED}
    SBX_USE_PROFILER=${SBX_USE_PROFILER}
    $<$<BOOL:${SBX_USE_PROFILER}>:BUILD_WITH_EASY_PROFILER>
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

# if(NOT MINGW)
#   list(APPEND _LINK_OPTIONS -fsanitize=address,undefined)
# endif()

if(MINGW)
  list(APPEND _LINK_OPTIONS -Wl,--disable-dynamicbase,--default-image-base-low)
endif()

target_link_options(
  ${PROJECT_NAME}
  PUBLIC
    ${_LINK_OPTIONS}
)
