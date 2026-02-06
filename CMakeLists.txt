cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

project(libsbx VERSION 0.1.0 LANGUAGES CXX)

# string(TOUPPER ${CMAKE_PROJECT_NAME} _CMAKE_PROJECT_NAME_UPPER)
# We use SBX instead of LIBSBX since its the convention for macros in the project.
set(CMAKE_PROJECT_NAME_UPPER "SBX" CACHE STRING "CMAKE_PROJECT_NAME_UPPER" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# if (NOT DEFINED ENV{VULKAN_SDK})
#   message(FATAL_ERROR "VULKAN_SDK environment variable not set. Please set it to the path of your Vulkan SDK installation.")
# endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies" "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies")

option(SBX_BUILD_DEMO "Build demo" ON)
message(STATUS "SBX_BUILD_DEMO: ${SBX_BUILD_DEMO}")

option(SBX_BUILD_SHARED "Build shared library" OFF)
message(STATUS "SBX_BUILD_SHARED: ${SBX_BUILD_SHARED}")

option(SBX_BUILD_TESTS "Build tests" ON)
message(STATUS "SBX_BUILD_TESTS: ${SBX_BUILD_TESTS}")

option(SBX_BUILD_BENCHMARKS "Build benchmarks" ON)
message(STATUS "SBX_BUILD_BENCHMARKS: ${SBX_BUILD_BENCHMARKS}")

if(${SBX_BUILD_TESTS})
  enable_testing()
endif()

option(SBX_CONSTEXPR_ENABLED "Enable constexpr" On)
message(STATUS "SBX_CONSTEXPR_ENABLED: ${SBX_CONSTEXPR_ENABLED}")

set(_USE_PROFILER_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_USE_PROFILER_DEFAULT ON)
endif()

option(SBX_USE_PROFILER "Use profiler" ${_USE_PROFILER_DEFAULT})
message(STATUS "SBX_USE_PROFILER: ${SBX_USE_PROFILER}")

set(_MEMORY_TRACKING OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_MEMORY_TRACKING ON)
endif()

option(SBX_MEMORY_TRACKING "Use CPU memory tracking" ${_MEMORY_TRACKING})
message(STATUS "SBX_MEMORY_TRACKING: ${SBX_MEMORY_TRACKING}")

set(_USE_ASAN_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_USE_ASAN_DEFAULT ON)
endif()

option(SBX_USE_ASAN "Use ASAN" ${_USE_ASAN_DEFAULT})
message(STATUS "SBX_USE_ASAN: ${SBX_USE_ASAN}")

# Default component list (override from CLI with -DLIBSBX_COMPONENTS="...;...")
set(LIBSBX_COMPONENTS units utility io memory signals containers assets bitmaps math ecs core audio devices gizmos graphics scenes scripting models animations particles post ui sprites physics editor CACHE STRING "libsbx components to include (semicolon-separated)")

foreach(_c IN LISTS LIBSBX_COMPONENTS)
  string(TOUPPER "${_c}" _C_UP)
  option(LIBSBX_ENABLE_${_C_UP} "Build libsbx component '${_c}'" ON)
endforeach()

string(TIMESTAMP COMPILE_TIMESTAMP %Y%m%d-%H%M%S)
set(SBX_COMPILE_TIMESTAMP "\"${COMPILE_TIMESTAMP}\"" CACHE STRING "Compile timestamp" FORCE)
add_compile_definitions(SBX_COMPILE_TIMESTAMP=${SBX_COMPILE_TIMESTAMP})

# Get current commit hash
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SBX_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SBX_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(SBX_COMMIT_HASH "unknown")
  set(SBX_BRANCH "unknown")
endif()

add_compile_definitions(
  SBX_COMMIT_HASH="${SBX_COMMIT_HASH}"
  SBX_BRANCH="${SBX_BRANCH}"
)

message(STATUS "Git branch: ${SBX_BRANCH}")
message(STATUS "Git commit: ${SBX_COMMIT_HASH}")

set(_libsbx_targets)

foreach(_c IN LISTS LIBSBX_COMPONENTS)
  string(TOUPPER "${_c}" _C_UP)
  set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/libsbx-${_c}")

  if(LIBSBX_ENABLE_${_C_UP} AND EXISTS "${_dir}/CMakeLists.txt")
    add_subdirectory("${_dir}")
    list(APPEND _libsbx_targets "libsbx::${_c}")
  endif()
endforeach()

add_library(libsbx INTERFACE)
add_library(libsbx::libsbx ALIAS libsbx)

target_include_directories(
  libsbx
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:install/include>
)

target_link_libraries(
  libsbx
  INTERFACE 
    ${_libsbx_targets}
)

if (${SBX_BUILD_DEMO})
  add_subdirectory(demo)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  EXPORT libsbxTargets
  NAMESPACE libsbx::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)

install(
  TARGETS libsbx
  EXPORT libsbxTargets
)

write_basic_package_version_file(
  libsbxConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/config/libsbxConfig.cmake.in
  ${CMAKE_BINARY_DIR}/libsbxConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)

install(
  FILES
    ${CMAKE_BINARY_DIR}/libsbxConfig.cmake
    ${CMAKE_BINARY_DIR}/libsbxConfigVersion.cmake
  DESTINATION 
    ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)
