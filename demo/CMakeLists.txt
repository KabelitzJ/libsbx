project(demo VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

find_package(easy_profiler CONFIG REQUIRED)

add_executable(${PROJECT_NAME})

# if(WIN32)
#   message(STATUS "Using WIN32 shell")

#   find_program(POWERSHELL_PATH NAMES pwsh)

#   message(STATUS "POWERSHELL_PATH: ${POWERSHELL_PATH}")

#   add_custom_target(
#     shaders
#     COMMAND ${POWERSHELL_PATH} -ExecutionPolicy Bypass "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.ps1" -directory "${PROJECT_SOURCE_DIR}/assets/shaders/"
#   )
# elseif(UNIX)
#   message(STATUS "Using UNIX shell")
#   find_program(BASH_PATH NAMES bash)

#   message(STATUS "BASH_PATH: ${BASH_PATH}")

#   add_custom_target(
#     shaders
#     COMMAND ${BASH_PATH} "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh" "${PROJECT_SOURCE_DIR}/assets/shaders/"
#   )
# else()
#   message(FATAL_ERROR "Unsupported platform detected")
# endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(
  compiled_shaders
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py" "${PROJECT_SOURCE_DIR}/assets/shaders/"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Compiling shaders for ${PROJECT_NAME}..."
)

add_dependencies(${PROJECT_NAME} compiled_shaders)

target_sources(
  ${PROJECT_NAME}
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/line.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/pipeline.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/vertex.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/terrain_subrenderer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/terrain_module.hpp"
    # "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/voronoi.hpp"
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/line.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/demo.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/vertex.cpp"
)

target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE
    LIBSBX_IMGUI_DOCKING=$<IF:$<BOOL:${LIBSBX_IMGUI_DOCKING}>,1,0>
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
    easy_profiler
  PRIVATE
    libsbx::libsbx
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

set(DOTNET_OUT_DIR ${CMAKE_BINARY_DIR}/_dotnet)

set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/Demo)
set(DEMO_PROJECT ${DEMO_DIR}/Demo.csproj)
file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS ${DEMO_DIR}/**/*.cs)

add_custom_target(
  scripting_demo
  ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_OUT_DIR}
  COMMAND dotnet build ${DEMO_PROJECT} -c $<CONFIG> -o ${DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${DEMO_DIR}
  COMMENT "Building managed assembly with dotnet build"
)

add_dependencies(${PROJECT_NAME} scripting_demo)

add_dependencies(scripting_demo scripting_managed scripting_core)
