cmake_minimum_required(VERSION 3.25)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

project(libsbx VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_PROJECT_NAME_UPPER "SBX" CACHE STRING "CMAKE_PROJECT_NAME_UPPER" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies" "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies")

include(cmake/fetch_dependencies.cmake)

fetch_dependencies("${CMAKE_SOURCE_DIR}/dependencies.json" "${CMAKE_BINARY_DIR}/dependencies")

# Operating system
if(WIN32)
  set(SBX_OPERATING_SYSTEM "WIN32")
elseif(APPLE)
  set(SBX_OPERATING_SYSTEM "APPLE")
elseif(UNIX)
  set(SBX_OPERATING_SYSTEM "LINUX")
else()
  set(SBX_OPERATING_SYSTEM "UNKNOWN")
  message(WARNING "Unknown operating system")
endif()

# Compiler
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(SBX_COMPILER "CLANG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(SBX_COMPILER "GNU")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SBX_COMPILER "MSC")
else()
  set(SBX_COMPILER "UNKNOWN")
  message(WARNING "Unknown compiler")
endif()

# Build type

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SBX_BUILD_TYPE "DEBUG")
else()
  set(SBX_BUILD_TYPE "RELEASE")
endif()

# Options

option(SBX_BUILD_DEMO "Build demo" ON)
option(SBX_BUILD_TESTS "Build tests" ON)
option(SBX_BUILD_SHARED "Build shared library" OFF)


message(STATUS "SBX_BUILD_DEMO: ${SBX_BUILD_DEMO}")
message(STATUS "SBX_BUILD_TESTS: ${SBX_BUILD_TESTS}")
message(STATUS "SBX_BUILD_SHARED: ${SBX_BUILD_SHARED}")

# Build metadata

string(TIMESTAMP COMPILE_TIMESTAMP %Y%m%d-%H%M%S)
set(SBX_COMPILE_TIMESTAMP "${COMPILE_TIMESTAMP}" CACHE STRING "Compile timestamp" FORCE)

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

# set(SBX_DATA_DIR ${CMAKE_BINARY_DIR}/data)

# set(SBX_SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
# set(SBX_SHADER_OUT_DIR ${SBX_DATA_DIR}/shaders)

# set(DOTNET_OUT_DIR ${SBX_DATA_DIR}/dotnet)

# External dependencies

# find_package(fmt CONFIG REQUIRED)
# find_package(spdlog CONFIG REQUIRED)
# find_package(range-v3 CONFIG REQUIRED)
# find_package(lz4 CONFIG REQUIRED)
# find_package(zstd CONFIG REQUIRED)
# find_package(magic_enum CONFIG REQUIRED)
# find_package(yaml-cpp CONFIG REQUIRED)
# find_package(stb CONFIG REQUIRED)
# find_package(Vulkan REQUIRED)
# find_package(spirv-cross CONFIG REQUIRED)
# find_package(VulkanMemoryAllocator CONFIG REQUIRED)
# find_package(nlohmann_json CONFIG REQUIRED)
find_package(slang REQUIRED MODULE)
# find_package(glfw3 CONFIG REQUIRED)
# find_package(tinyobjloader CONFIG REQUIRED)
# find_package(base64 CONFIG REQUIRED)
# find_package(tsl-robin-map CONFIG REQUIRED)
# find_package(assimp CONFIG REQUIRED)
# find_package(meshoptimizer CONFIG REQUIRED)
# find_package(imgui CONFIG REQUIRED)
# find_package(implot CONFIG REQUIRED)
# find_package(imnodes CONFIG REQUIRED)
# find_package(imguizmo CONFIG REQUIRED)
# find_package(freetype CONFIG REQUIRED)
# find_package(drwav CONFIG REQUIRED)
# find_package(OpenAL CONFIG REQUIRED)
# find_package(sol2 CONFIG REQUIRED)
find_package(Nethost REQUIRED MODULE)
# find_package(Tracy CONFIG REQUIRED)

# option(GLFW_BUILD_EXAMPLES "" OFF)
# option(GLFW_BUILD_TESTS "" OFF)
# option(GLFW_BUILD_DOCS "" OFF)
# option(GLFW_INSTALL "" OFF)
# option(GLFW_DOCUMENT_INTERNALS "" OFF)
# option(SPIRV_CROSS_CLI "" OFF)
# option(SPIRV_CROSS_ENABLE_TESTS "" OFF)

# Configure version header

configure_file("${CMAKE_SOURCE_DIR}/libsbx/version.hpp.in" "${CMAKE_BINARY_DIR}/generated/libsbx/version.hpp" @ONLY)

# Library target

if(SBX_BUILD_SHARED)
  add_library(libsbx SHARED)
else()
  add_library(libsbx STATIC)
endif()

add_library(libsbx::libsbx ALIAS libsbx)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# add_custom_target(
#   compiled_shaders_libsbx ALL
#   COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py
#     ${SBX_SHADER_SRC_DIR}
#     --output ${SBX_SHADER_OUT_DIR}
#     --manifest ${SBX_SHADER_SRC_DIR}/manifest.txt
#   COMMENT "Compiling shaders for ${PROJECT_NAME}..."
# )

# add_custom_target(
#   staged_shaders_libsbx ALL
#   COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${SBX_SHADER_SRC_DIR} ${SBX_SHADER_OUT_DIR}
#   DEPENDS compiled_shaders_libsbx
#   COMMENT "Staging shader sources for ${PROJECT_NAME}..."
# )

# add_dependencies(${PROJECT_NAME} staged_shaders_libsbx)

target_sources(
  libsbx
  PRIVATE
    "${CMAKE_SOURCE_DIR}/libsbx/core/core.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/engine.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/platform/platform_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/platform/window.cpp"
  PUBLIC
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_SOURCE_DIR}"
    FILES
      "${CMAKE_SOURCE_DIR}/libsbx/core/core.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/engine.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/platform_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/window.hpp"
  FILE_SET generated TYPE HEADERS
    BASE_DIRS "${CMAKE_BINARY_DIR}/generated"
    FILES
      "${CMAKE_BINARY_DIR}/generated/libsbx/version.hpp"
)

target_include_directories(
  libsbx
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(
  libsbx
  PUBLIC
    fmt::fmt
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    glfw
    Nethost::nethost
    slang::slang
    stb::stb
    meshoptimizer
    Vulkan::Headers
    GPUOpen::VulkanMemoryAllocator
)

set_target_properties(
  libsbx
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME sbx
)

if(MSVC)
  set_target_properties(libsbx PROPERTIES PREFIX "lib")
endif()

target_compile_features(libsbx PUBLIC cxx_std_23)

target_compile_definitions(
  libsbx
  PUBLIC
    SBX_COMPILE_TIMESTAMP="${SBX_COMPILE_TIMESTAMP}"
    SBX_COMMIT_HASH="${SBX_COMMIT_HASH}"
    SBX_BRANCH="${SBX_BRANCH}"
    SBX_OPERATING_SYSTEM_${SBX_OPERATING_SYSTEM}
    SBX_COMPILER_${SBX_COMPILER}
    SBX_BUILD_TYPE_${SBX_BUILD_TYPE}
  PRIVATE
    GLFW_INCLUDE_VULKAN
)

target_compile_options(
  libsbx
  PRIVATE
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

set(_sbx_link_options)
set(_sbx_compile_options)

if(MINGW)
  list(APPEND _sbx_link_options -Wl,--disable-dynamicbase,--default-image-base-low)
endif()

target_compile_options(libsbx PRIVATE ${_sbx_compile_options})
target_link_options(libsbx PUBLIC ${_sbx_link_options})

# Scripting: dotnet build targets

# set(SBX_MANAGED_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Managed)
# set(SBX_MANAGED_PROJECT ${SBX_MANAGED_DIR}/Sbx.Managed.csproj)

# add_custom_target(
#   sbx_scripting_managed ALL
#   COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_OUT_DIR}
#   COMMAND dotnet publish ${SBX_MANAGED_PROJECT}
#     -c $<CONFIG>
#     --self-contained false
#     -p:UseAppHost=false
#     -o ${DOTNET_OUT_DIR}
#   WORKING_DIRECTORY ${SBX_MANAGED_DIR}
#   COMMENT "Publishing Sbx.Managed assembly (framework-dependent)"
# )

# add_dependencies(libsbx sbx_scripting_managed)

# set(SBX_CORE_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Core)
# set(SBX_CORE_PROJECT ${SBX_CORE_DIR}/Sbx.Core.csproj)

# add_custom_target(
#   sbx_scripting_core ALL
#   COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_OUT_DIR}
#   COMMAND dotnet publish ${SBX_CORE_PROJECT}
#     -c $<CONFIG>
#     --self-contained false
#     -p:UseAppHost=false
#     -o ${DOTNET_OUT_DIR}
#   WORKING_DIRECTORY ${SBX_CORE_DIR}
#   COMMENT "Publishing Sbx.Core assembly (self-contained ${SBX_DOTNET_RID})"
# )

# add_dependencies(sbx_scripting_core sbx_scripting_managed)
# add_dependencies(libsbx sbx_scripting_core)

# Install

install(
  TARGETS libsbx
  EXPORT libsbxTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  FILE_SET headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILE_SET generated DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT libsbxTargets
  NAMESPACE libsbx::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
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

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/modules/
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx/modules
  FILES_MATCHING PATTERN "Find*.cmake"
)

# install(CODE "
#   if(NOT EXISTS \"${SBX_SHADER_OUT_DIR}/manifest.txt\")
#     message(FATAL_ERROR \"Engine data not staged. Run 'cmake --build' before 'cmake --install'.\")
#   endif()
# ")

# install(
#   DIRECTORY ${SBX_DATA_DIR}/
#   DESTINATION ${CMAKE_INSTALL_DATADIR}/libsbx
#   USE_SOURCE_PERMISSIONS
#   PATTERN "*.pdb" EXCLUDE
# )


# install(
#   FILES $<TARGET_FILE:slang::slang>
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

# Demo

if(SBX_BUILD_DEMO AND EXISTS "${CMAKE_SOURCE_DIR}/demo")
  add_subdirectory(demo)
endif()
