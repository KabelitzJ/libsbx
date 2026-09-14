project(editor VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

add_executable(${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_camera.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_module.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_ui_layer.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/scene_persistence.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_window.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_state.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/memory.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/play_mode_controller.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/command_stack.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/composite_command.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/scene_commands.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/script_commands.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_reparent.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_prefab_menu.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_asset_pickers.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_component_sections.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_component_registry.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_script_section.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_asset_editors.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_prefab_edit.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_metadata.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_tree_view.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_grid_view.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_file_ops.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_import_queue.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/logger_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/animation_graph_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/shader_graph_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/navigation_panel.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_camera.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_transform_gizmo.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_view_gizmo.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_overlays.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_picking.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/vector_fields.cpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/layer_fields.cpp"
  PUBLIC
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/application.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_camera.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_ui_layer.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/editor_state.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/play_mode_controller.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/command.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/scene_access.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/command_stack.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/composite_command.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/component_commands.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/prefab_override.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/scene_commands.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/commands/script_commands.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/editor_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/hierarchy_prefab_menu.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_asset_pickers.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_component_sections.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_component_registry.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/inspector_script_section.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_metadata.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/asset_browser_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/logger_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/animation_graph_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/shader_graph_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/panels/navigation_panel.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_window.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_camera.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_transform_gizmo.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_view_gizmo.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_overlays.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/viewport_picking.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/text_field.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/inline_rename.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/vector_fields.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/layer_fields.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/drag_session.hpp"
    "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/widgets/asset_path.hpp"
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

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
