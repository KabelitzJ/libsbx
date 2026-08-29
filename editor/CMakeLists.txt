project(editor VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

add_executable(${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_module.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_ui_layer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_state.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/fly_camera.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/memory.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/play_mode_controller.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/logger_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_camera.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_gizmo.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_picking.cpp"
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_ui_layer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_state.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/fly_camera.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/play_mode_controller.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/editor_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/logger_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_camera.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_gizmo.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_picking.hpp"
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

add_dependencies(${PROJECT_NAME} sbx_scripting_managed sbx_scripting_core)
