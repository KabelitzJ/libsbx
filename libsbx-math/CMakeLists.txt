project(math VERSION 0.1.0 LANGUAGES CXX)

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
find_package(yaml-cpp CONFIG REQUIRED)
# find_package(glm CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/algorithm.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/angle.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/box.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/color.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/math.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix3x3.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix4x4.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix_cast.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix_cast.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/quaternion.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/random.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/ray.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/uuid.cpp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector2.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector3.ipp"
    "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector4.ipp"
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/algorithm.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/angle.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/box.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/color.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/concepts.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/constants.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/ieee_754_float.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/math.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix3x3.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix4x4.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/matrix_cast.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/noise.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/plane.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/quaternion.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/random.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/ray.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/smooth_value.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/sphere.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/traits.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/uuid.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector2.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector3.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/vector4.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/version.hpp"
      "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}/volume.hpp"
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
    yaml-cpp::yaml-cpp
    # glm::glm
    range-v3::range-v3
    # Internal dependencies
    libsbx::utility
    libsbx::core
    libsbx::io
    libsbx::ecs
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
