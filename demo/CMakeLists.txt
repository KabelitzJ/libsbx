project(demo VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

add_executable(${PROJECT_NAME})

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(DEMO_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(DEMO_SHADER_OUT_DIR ${SBX_DATA_DIR}/demo/shaders)

add_custom_target(
  compiled_shaders_demo ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py
    ${DEMO_SHADER_SRC_DIR}
    --output ${DEMO_SHADER_OUT_DIR}
    --manifest ${DEMO_SHADER_SRC_DIR}/manifest.txt
    -I ${SBX_SHADER_SRC_DIR}
  COMMENT "Compiling shaders for ${PROJECT_NAME}..."
)

add_custom_target(
  staged_shaders_demo ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${DEMO_SHADER_SRC_DIR} ${DEMO_SHADER_OUT_DIR}
  DEPENDS compiled_shaders_demo
  COMMENT "Staging shader sources for ${PROJECT_NAME}..."
)

add_dependencies(${PROJECT_NAME} staged_shaders_demo)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/demo.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/terrain_module.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/terrain_subrenderer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/terrain/water_subrenderer.cpp"
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
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
    SBX_CONSTEXPR_ENABLED=$<IF:$<BOOL:${SBX_CONSTEXPR_ENABLED}>,1,0>
)

target_compile_features(
  ${PROJECT_NAME}
  PUBLIC
    cxx_std_23
)

target_compile_options(
  ${PROJECT_NAME}
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

set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/Demo)
set(DEMO_PROJECT ${DEMO_DIR}/Demo.csproj)
set(DEMO_DOTNET_OUT_DIR ${CMAKE_BINARY_DIR}/data/dotnet/demo)

file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS ${DEMO_DIR}/**/*.cs)

add_custom_target(
  scripting_demo ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DEMO_DOTNET_OUT_DIR}
  COMMAND dotnet publish ${DEMO_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -p:BuildProjectReferences=false
    -o ${DEMO_DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${DEMO_DIR}
  COMMENT "Publishing Demo assembly"
)

add_dependencies(scripting_demo sbx_scripting_managed sbx_scripting_core)
add_dependencies(${PROJECT_NAME} scripting_demo)
