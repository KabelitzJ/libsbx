project(ecs VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)

configure_file("${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp.in" "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp" @ONLY)

if(${SBX_BUILD_SHARED})
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

add_library(${CMAKE_PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/ecs.cpp"
  PUBLIC
    FILE_SET HEADERS
    FILES
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/component.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/detail/registry_storage_iterator.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/detail/sparse_set_iterator.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/detail/storage_iterator.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/detail/view_iterator.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/ecs.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/entity.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/meta.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/range.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/registry.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/sparse_set.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/storage.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/view.hpp"
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
    # Internal dependencies
    libsbx::utility
    libsbx::core
    libsbx::memory
    libsbx::containers
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
  PRIVATE
    SBX_CONSTEXPR_ENABLED=${SBX_CONSTEXPR_ENABLED}
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

if(${SBX_BUILD_TESTS})
  add_subdirectory(tests)
endif()
