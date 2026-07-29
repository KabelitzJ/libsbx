project(editor VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

add_executable(${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_module.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/fly_camera.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/memory.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/backends/imgui_impl_glfw.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/backends/imgui_impl_vulkan.cpp"
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/fly_camera.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/backends/imgui_impl_glfw.h"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/backends/imgui_impl_vulkan.h"
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
    imgui::imgui
    imguizmo::imguizmo
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

set(EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/Editor)
set(EDITOR_PROJECT ${EDITOR_DIR}/Editor.csproj)
set(EDITOR_DOTNET_OUT_DIR ${CMAKE_BINARY_DIR}/dotnet/editor)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS ${EDITOR_DIR}/**/*.cs)

add_custom_target(
  editor_scripting ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EDITOR_DOTNET_OUT_DIR}
  COMMAND dotnet publish ${EDITOR_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -p:BuildProjectReferences=false
    -o ${EDITOR_DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${EDITOR_DIR}
  COMMENT "Publishing Editor assembly"
)

add_dependencies(editor_scripting sbx_scripting_managed sbx_scripting_core)
add_dependencies(${PROJECT_NAME} editor_scripting)
