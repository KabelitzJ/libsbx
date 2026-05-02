project(editor VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(imnodes CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

add_executable(${PROJECT_NAME})

# find_package(Python3 REQUIRED COMPONENTS Interpreter)

# add_custom_target(
#   compiled_shaders_editor
#   COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py" "${PROJECT_SOURCE_DIR}/assets/shaders/"
#   WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
#   COMMENT "Compiling shaders for ${PROJECT_NAME}..."
# )

# add_dependencies(${PROJECT_NAME} compiled_shaders_editor)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/bindings/1.91.8-docking/imgui_impl_vulkan.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/bindings/1.91.8-docking/imgui_impl_glfw.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_context.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_subrenderer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/log_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/viewport_panel.cpp"
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/renderer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/bindings/imgui.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_context.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_subrenderer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/log_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/viewport_panel.hpp"
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
    imgui::imgui
    implot::implot
    imnodes::imnodes
    imguizmo::imguizmo
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

set(EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/Editor)
set(EDITOR_PROJECT ${EDITOR_DIR}/Editor.csproj)
set(EDITOR_DOTNET_OUT_DIR ${CMAKE_BINARY_DIR}/data/dotnet/editor)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS ${EDITOR_DIR}/**/*.cs)

add_custom_target(
  scripting_editor ALL
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

add_dependencies(scripting_editor sbx_scripting_managed sbx_scripting_core)
add_dependencies(${PROJECT_NAME} scripting_editor)
