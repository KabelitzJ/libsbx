// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <imgui_node_editor.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/asset_browser_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/logger_panel.hpp>
#include <editor/panels/inspector_panel.hpp>
#include <editor/panels/animation_graph_panel.hpp>
#include <editor/panels/shader_graph_panel.hpp>
#include <editor/panels/navigation_panel.hpp>
#include <editor/panels/statistics_panel.hpp>
#include <editor/panels/import_window.hpp>
#include <editor/panels/scene_renderer_panel.hpp>
#include <editor/panels/project_settings_panel.hpp>
#include <editor/panels/preferences_panel.hpp>

#include <editor/widgets/layer_fields.hpp>
#include <editor/widgets/text_field.hpp>

#include <editor/editor_actions.hpp>

#include <editor/viewport_window.hpp>
#include <editor/viewport_view_gizmo.hpp>
#include <editor/viewport_placement.hpp>

#include <editor/editor_module.hpp>
#include <editor/node_actions.hpp>

#include <editor/commands/scene_commands.hpp>

#include <editor/probe_baking.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/physics/physics_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

namespace editor {

static auto viewport_sampler_create_info() -> sbx::graphics::sampler::create_info {
  return sbx::graphics::sampler::create_info{
    .mag_filter = sbx::graphics::filter::linear,
    .min_filter = sbx::graphics::filter::linear,
    .mipmap_mode = sbx::graphics::mipmap_mode::linear,
    .address_mode_u = sbx::graphics::address_mode::clamp_to_edge,
    .address_mode_v = sbx::graphics::address_mode::clamp_to_edge,
    .address_mode_w = sbx::graphics::address_mode::clamp_to_edge,
    .max_anisotropy = 1.0f,
    .max_lod = sbx::graphics::lod_clamp::none,
    .name = "Editor Viewport Sampler"
  };
}

editor_ui_layer::editor_ui_layer()
: _sampler{viewport_sampler_create_info()} {
  _upload_fonts();

  sbx::core::engine::get_module<sbx::render::ui_module>().apply_default_style();

  _state.thumbnails = sbx::memory::make_observer(&_thumbnails);

  _create_panels();

  _register_actions();

  auto& window = sbx::core::engine::get_module<sbx::platform::platform_module>().window();

  _files_dropped = window.on_files_dropped().connect_scoped([this](const sbx::platform::files_dropped_event& event) {
    _state.dropped_files.insert(_state.dropped_files.end(), event.paths.begin(), event.paths.end());
  });
}

// Whether @p candidate is @p name's window or one of its child windows.
auto is_editor_window(const ImGuiWindow* candidate, const char* name) -> bool {
  const auto* window = ImGui::FindWindowByName(name);

  return window != nullptr && candidate != nullptr && candidate->RootWindow == window;
}

auto editor_ui_layer::_handle_shortcuts() -> void {
  const auto& context = *ImGui::GetCurrentContext();

  const auto viewport_focused = is_editor_window(context.NavWindow, viewport_window_name);
  const auto viewport_active = viewport_focused || is_editor_window(context.HoveredWindow, viewport_window_name);

  auto contexts = std::vector<action_context>{};

  if (viewport_focused || is_editor_window(context.NavWindow, hierarchy_panel::window_name)) {
    contexts.push_back(action_context::scene);
  }

  if (viewport_active) {
    contexts.push_back(action_context::viewport);
  }

  if (is_editor_window(context.NavWindow, asset_browser_panel::window_name)) {
    contexts.push_back(action_context::asset_browser);
  }

  // While the game runs, the Viewport's keys belong to it; only Play, Pause and Stop still reach the editor there.
  const auto is_playing = sbx::core::engine::get_module<editor::editor_module>().play_state() != editor::play_state::edit;

  _state.actions.dispatch(contexts, is_playing && viewport_active);
}

auto editor_ui_layer::_register_actions() -> void {
  auto& actions = _state.actions;

  const auto active_scene = []() -> sbx::scenes::scene& {
    return sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
  };

  const auto play_state = [] {
    return sbx::core::engine::get_module<editor::editor_module>().play_state();
  };

  const auto is_editing = [play_state] {
    return play_state() == editor::play_state::edit;
  };

  const auto has_nodes = [this] {
    return _state.selected_node_count() > 0u;
  };

  // Scene files.
  actions.add(editor_action{.id = "scene.new", .label = "New Scene", .icon = ICON_MDI_FILE_PLUS, .category = "Scene", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_N}, .is_enabled = is_editing, .run = [this] { new_scene(); }});
  actions.add(editor_action{.id = "scene.open", .label = "Open Scene...", .icon = ICON_MDI_FOLDER_OPEN, .category = "Scene", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_O}, .is_enabled = is_editing, .run = [this] { _open_open_scene_dialog(); }});
  actions.add(editor_action{.id = "scene.open_additive", .label = "Open Scene Additive...", .icon = ICON_MDI_FOLDER_PLUS, .category = "Scene", .is_enabled = is_editing, .run = [this] { _open_open_scene_dialog(true); }});
  actions.add(editor_action{.id = "scene.save", .label = "Save Scene", .icon = ICON_MDI_CONTENT_SAVE, .category = "Scene", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_S}, .is_enabled = is_editing, .run = [this] { _save(); }});
  actions.add(editor_action{.id = "scene.save_as", .label = "Save Scene As...", .icon = ICON_MDI_CONTENT_SAVE_EDIT, .category = "Scene", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S}, .is_enabled = is_editing, .run = [this] { _open_save_as_dialog(sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene_handle()); }});
  actions.add(editor_action{.id = "scene.save_all", .label = "Save All Scenes", .icon = ICON_MDI_CONTENT_SAVE_ALL, .category = "Scene", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_S, ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_S}, .is_enabled = is_editing, .run = [this] { _save_all(); }});
  actions.add(editor_action{.id = "scene.quit", .label = "Quit", .icon = ICON_MDI_EXIT_TO_APP, .category = "Scene", .run = [this] { request_quit(); }});

  // Editing.
  actions.add(editor_action{.id = "edit.undo", .label = "Undo", .icon = ICON_MDI_UNDO, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_Z}, .is_enabled = [this] { return _state.can_undo(); }, .run = [this, active_scene] { _state.undo(active_scene()); }, .menu_label = [this] { return fmt::format("Undo {}", _state.undo_label()); }});
  actions.add(editor_action{.id = "edit.redo", .label = "Redo", .icon = ICON_MDI_REDO, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiMod_Ctrl | ImGuiKey_Y}, .is_enabled = [this] { return _state.can_redo(); }, .run = [this, active_scene] { _state.redo(active_scene()); }, .menu_label = [this] { return fmt::format("Redo {}", _state.redo_label()); }});
  actions.add(editor_action{.id = "edit.copy", .label = "Copy", .icon = ICON_MDI_CONTENT_COPY, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_C}, .context = action_context::scene, .is_enabled = has_nodes, .run = [this, active_scene] { copy_selection(_state, active_scene()); }});
  actions.add(editor_action{.id = "edit.paste", .label = "Paste", .icon = ICON_MDI_CONTENT_PASTE, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_V}, .context = action_context::scene, .is_enabled = [this] { return !_state.node_clipboard.empty(); }, .run = [this, active_scene] { paste_clipboard(_state, active_scene()); }});
  actions.add(editor_action{.id = "edit.duplicate", .label = "Duplicate", .icon = ICON_MDI_CONTENT_DUPLICATE, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_D}, .context = action_context::scene, .is_enabled = has_nodes, .run = [this, active_scene] { duplicate_selection(_state, active_scene()); }});
  actions.add(editor_action{.id = "edit.delete", .label = "Delete", .icon = ICON_MDI_DELETE, .category = "Edit", .default_chords = {ImGuiKey_Delete}, .context = action_context::scene, .is_enabled = has_nodes, .run = [this, active_scene] { delete_selection(_state, active_scene()); }});
  actions.add(editor_action{.id = "edit.rename", .label = "Rename", .icon = ICON_MDI_PENCIL, .category = "Edit", .default_chords = {ImGuiKey_F2}, .context = action_context::scene, .is_enabled = [this] { return _state.selected_node_count() == 1u; }});
  actions.add(editor_action{.id = "edit.add_child", .label = "Add Child Node", .icon = ICON_MDI_PLUS, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiKey_A}, .context = action_context::scene, .is_enabled = is_editing, .run = [this, active_scene] {
    auto& scene = active_scene();
    const auto parent = _state.selected_node(scene);

    auto command = std::make_unique<create_node_command>(parent.is_valid() ? std::optional{static_cast<sbx::math::uuid>(parent.id())} : std::nullopt);
    auto* created = command.get();

    _state.push_command(scene, std::move(command));
    _state.select_node(scene.find(created->id()));
  }});
  actions.add(editor_action{.id = "edit.history", .label = "History", .icon = ICON_MDI_HISTORY, .category = "Edit", .is_checked = [this] { return _history_panel->is_open; }, .run = [this] { _history_panel->is_open = !_history_panel->is_open; }});
  actions.add(editor_action{.id = "edit.select_all", .label = "Select All", .icon = ICON_MDI_SELECT_ALL, .category = "Edit", .context = action_context::scene, .run = [this, active_scene] {
    auto ids = std::vector<sbx::math::uuid>{};

    for (auto&& [entity, id] : active_scene().query<sbx::scenes::id>().each()) {
      ids.push_back(static_cast<sbx::math::uuid>(id));
    }

    _state.set_node_selection(std::move(ids));
  }});
  actions.add(editor_action{.id = "edit.deselect", .label = "Deselect", .icon = ICON_MDI_SELECT_OFF, .category = "Edit", .default_chords = {ImGuiKey_Escape}, .is_enabled = [] { return !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId); }, .run = [this] { _state.clear_selection(); }});
  actions.add(editor_action{.id = "edit.focus_selection", .label = "Focus Selection", .icon = ICON_MDI_CROSSHAIRS_GPS, .category = "Edit", .default_chords = {ImGuiKey_F}, .context = action_context::scene, .is_enabled = [is_editing, has_nodes] { return is_editing() && has_nodes(); }, .run = [this, active_scene] { focus_or_follow_selection(_state, active_scene()); }});
  actions.add(editor_action{.id = "edit.align_transform_with_view", .label = "Align Transform with View", .icon = ICON_MDI_CAMERA_SWITCH, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_M}, .context = action_context::scene, .is_enabled = [is_editing, has_nodes] { return is_editing() && has_nodes(); }, .run = [this, active_scene] { align_selection_with_view(_state, active_scene(), true); }});
  actions.add(editor_action{.id = "edit.align_rotation_with_view", .label = "Align Rotation with View", .icon = ICON_MDI_ROTATE_ORBIT, .category = "Edit", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_F}, .context = action_context::scene, .is_enabled = [is_editing, has_nodes] { return is_editing() && has_nodes(); }, .run = [this, active_scene] { align_selection_with_view(_state, active_scene(), false); }});

  // Transform gizmo, like Godot's 3D editor (Q/W/E/R, T local space, Y snap); 1/2/3 kept from before.
  actions.add(editor_action{.id = "gizmo.select", .label = "Select Tool", .icon = ICON_MDI_CURSOR_DEFAULT, .category = "Viewport", .default_chords = {ImGuiKey_Q}, .context = action_context::viewport, .is_checked = [this] { return _state.current_gizmo_operation == gizmo_operation::select; }, .run = [this] { _state.current_gizmo_operation = gizmo_operation::select; }});
  actions.add(editor_action{.id = "gizmo.move", .label = "Move Tool", .icon = ICON_MDI_ARROW_ALL, .category = "Viewport", .default_chords = {ImGuiKey_W, ImGuiKey_1}, .context = action_context::viewport, .is_checked = [this] { return _state.current_gizmo_operation == gizmo_operation::translate; }, .run = [this] { _state.current_gizmo_operation = gizmo_operation::translate; }});
  actions.add(editor_action{.id = "gizmo.rotate", .label = "Rotate Tool", .icon = ICON_MDI_ROTATE_3D_VARIANT, .category = "Viewport", .default_chords = {ImGuiKey_E, ImGuiKey_2}, .context = action_context::viewport, .is_checked = [this] { return _state.current_gizmo_operation == gizmo_operation::rotate; }, .run = [this] { _state.current_gizmo_operation = gizmo_operation::rotate; }});
  actions.add(editor_action{.id = "gizmo.scale", .label = "Scale Tool", .icon = ICON_MDI_ARROW_EXPAND_ALL, .category = "Viewport", .default_chords = {ImGuiKey_R, ImGuiKey_3}, .context = action_context::viewport, .is_checked = [this] { return _state.current_gizmo_operation == gizmo_operation::scale; }, .run = [this] { _state.current_gizmo_operation = gizmo_operation::scale; }});
  actions.add(editor_action{.id = "gizmo.local_space", .label = "Use Local Space", .icon = ICON_MDI_AXIS_ARROW, .category = "Viewport", .default_chords = {ImGuiKey_T}, .context = action_context::viewport, .is_checked = [this] { return _state.current_gizmo_mode == gizmo_mode::local; }, .run = [this] {
    _state.current_gizmo_mode = (_state.current_gizmo_mode == gizmo_mode::local) ? gizmo_mode::world : gizmo_mode::local;
  }});
  actions.add(editor_action{.id = "gizmo.snap", .label = "Use Snap", .icon = ICON_MDI_MAGNET, .category = "Viewport", .default_chords = {ImGuiKey_Y}, .context = action_context::viewport, .is_checked = [this] { return _state.is_snap_enabled; }, .run = [this] { _state.is_snap_enabled = !_state.is_snap_enabled; }});

  // Numpad views like Godot's (Alt for the opposite side), and Focus Origin. Perspective only for now: orthographic needs renderer support.
  const auto view_action = [&actions, is_editing](std::string id, std::string label, ImGuiKeyChord chord, sbx::math::vector3 look_from) {
    actions.add(editor_action{.id = std::move(id), .label = std::move(label), .icon = ICON_MDI_AXIS_ARROW_INFO, .category = "View", .default_chords = {chord}, .context = action_context::viewport, .is_enabled = is_editing, .run = [look_from] { request_view_snap(look_from); }});
  };

  view_action("view.front", "Front View", ImGuiKey_Keypad1, sbx::math::vector3{0.0f, 0.0f, 1.0f});
  view_action("view.rear", "Rear View", ImGuiMod_Alt | ImGuiKey_Keypad1, sbx::math::vector3{0.0f, 0.0f, -1.0f});
  view_action("view.right", "Right View", ImGuiKey_Keypad3, sbx::math::vector3{1.0f, 0.0f, 0.0f});
  view_action("view.left", "Left View", ImGuiMod_Alt | ImGuiKey_Keypad3, sbx::math::vector3{-1.0f, 0.0f, 0.0f});
  view_action("view.top", "Top View", ImGuiKey_Keypad7, sbx::math::vector3{0.0f, 1.0f, 0.0f});
  view_action("view.bottom", "Bottom View", ImGuiMod_Alt | ImGuiKey_Keypad7, sbx::math::vector3{0.0f, -1.0f, 0.0f});

  actions.add(editor_action{.id = "view.focus_origin", .label = "Focus Origin", .icon = ICON_MDI_CROSSHAIRS, .category = "View", .default_chords = {ImGuiKey_O}, .context = action_context::viewport, .is_enabled = is_editing, .run = [] { focus_origin(); }});

  // Placement tools.
  actions.add(editor_action{.id = "edit.snap_to_floor", .label = "Snap to Floor", .icon = ICON_MDI_ARROW_COLLAPSE_DOWN, .category = "Edit", .default_chords = {ImGuiKey_PageDown}, .context = action_context::scene, .is_enabled = [is_editing, has_nodes] { return is_editing() && has_nodes(); }, .run = [this, active_scene] { snap_selection_to_floor(_state, active_scene()); }});
  actions.add(editor_action{.id = "view.align_drops", .label = "Align Drops to Surface", .icon = ICON_MDI_ARROW_UP_BOLD_BOX_OUTLINE, .category = "Viewport", .is_checked = [this] { return _state.align_drops_to_surface; }, .run = [this] { _state.align_drops_to_surface = !_state.align_drops_to_surface; }});
  actions.add(editor_action{.id = "view.ruler", .label = "Ruler", .icon = ICON_MDI_RULER, .category = "Viewport", .context = action_context::viewport, .is_enabled = is_editing, .is_checked = [this] { return _state.is_ruler_active; }, .run = [this] { _state.is_ruler_active = !_state.is_ruler_active; }});

  // Godot's camera Preview: the Viewport looks through the selected camera until toggled off.
  actions.add(editor_action{.id = "view.preview_camera", .label = "Preview Camera", .icon = ICON_MDI_CAMERA, .category = "View", .is_enabled = [this, is_editing, active_scene] {
    auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
    const auto selected = _state.selected_node(active_scene());

    return is_editing() && (editor_module.preview_camera() != sbx::math::uuid::nil() || (selected.is_valid() && selected.has_component<sbx::scenes::camera>()));
  }, .is_checked = [] { return sbx::core::engine::get_module<editor::editor_module>().preview_camera() != sbx::math::uuid::nil(); }, .run = [this, active_scene] {
    auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

    if (editor_module.preview_camera() != sbx::math::uuid::nil()) {
      editor_module.set_preview_camera(sbx::math::uuid::nil());
      return;
    }

    if (const auto selected = _state.selected_node(active_scene()); selected.is_valid() && selected.has_component<sbx::scenes::camera>()) {
      editor_module.set_preview_camera(selected.id());
    }
  }});

  // Viewport display.
  actions.add(editor_action{.id = "view.grid", .label = "Show Grid", .icon = ICON_MDI_GRID, .category = "View", .is_checked = [] { return sbx::core::engine::get_module<sbx::render::scene_renderer_module>().grid_enabled(); }, .run = [] {
    auto& renderer = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
    renderer.set_grid_enabled(!renderer.grid_enabled());
  }});
  actions.add(editor_action{.id = "view.wireframe", .label = "Wireframe", .icon = ICON_MDI_CUBE_OUTLINE, .category = "View", .is_checked = [] { return sbx::core::engine::get_module<sbx::render::scene_renderer_module>().wireframe_enabled(); }, .run = [] {
    auto& renderer = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
    renderer.set_wireframe_enabled(!renderer.wireframe_enabled());
  }});

  // Debug overlays: one toggle per physics/nav draw flag, plus the shadow cascade tint.
  const auto debug_toggle = [&actions](std::string id, std::string label, bool sbx::physics::debug_draw_flags::* flag) {
    actions.add(editor_action{.id = std::move(id), .label = std::move(label), .icon = ICON_MDI_BUG_OUTLINE, .category = "Debug", .is_checked = [flag] { return sbx::core::engine::get_module<sbx::physics::physics_module>().debug_draw_flags().*flag; }, .run = [flag] {
      auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();
      auto flags = physics_module.debug_draw_flags();
      flags.*flag = !(flags.*flag);
      physics_module.set_debug_draw_flags(flags);
    }});
  };

  debug_toggle("debug.colliders", "Physics Colliders", &sbx::physics::debug_draw_flags::colliders);
  debug_toggle("debug.broadphase", "Physics Broadphase", &sbx::physics::debug_draw_flags::broadphase);
  debug_toggle("debug.contacts", "Physics Contacts", &sbx::physics::debug_draw_flags::contacts);
  debug_toggle("debug.navmesh", "Navmesh", &sbx::physics::debug_draw_flags::navmesh);
  debug_toggle("debug.nav_agents", "Nav Agents", &sbx::physics::debug_draw_flags::nav_agents);

  actions.add(editor_action{.id = "debug.shadow_cascades", .label = "Shadow Cascades", .icon = ICON_MDI_BUG_OUTLINE, .category = "Debug", .is_checked = [] { return sbx::core::engine::get_module<sbx::render::scene_renderer_module>().shadow_cascade_debug_enabled(); }, .run = [] {
    auto& renderer = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
    renderer.set_shadow_cascade_debug_enabled(!renderer.shadow_cascade_debug_enabled());
  }});

  // Play mode, like Godot's F5/F7/F8; these still reach the editor while the game has the Viewport.
  actions.add(editor_action{.id = "play.play", .label = "Play", .icon = ICON_MDI_PLAY, .category = "Play", .default_chords = {ImGuiKey_F5}, .works_while_playing = true, .is_enabled = [play_state] { return play_state() != editor::play_state::playing; }, .run = [this, play_state] {
    auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

    if (play_state() == editor::play_state::edit) {
      _state.clear_selection();
      editor_module.enter_play_mode();
    } else {
      editor_module.toggle_pause();
    }
  }});
  actions.add(editor_action{.id = "play.pause", .label = "Pause", .icon = ICON_MDI_PAUSE, .category = "Play", .default_chords = {ImGuiKey_F7}, .works_while_playing = true, .is_enabled = [play_state] { return play_state() != editor::play_state::edit; }, .is_checked = [play_state] { return play_state() == editor::play_state::paused; }, .run = [] { sbx::core::engine::get_module<editor::editor_module>().toggle_pause(); }});
  actions.add(editor_action{.id = "play.stop", .label = "Stop", .icon = ICON_MDI_STOP, .category = "Play", .default_chords = {ImGuiKey_F8}, .works_while_playing = true, .is_enabled = [play_state] { return play_state() != editor::play_state::edit; }, .run = [this] {
    _state.clear_selection();
    sbx::core::engine::get_module<editor::editor_module>().exit_play_mode();
  }});

  // Project and editor.
  actions.add(editor_action{.id = "project.settings", .label = "Project Settings", .icon = ICON_MDI_COG, .category = "Project", .run = [this] { _project_settings_panel->is_open = true; }});
  actions.add(editor_action{.id = "project.recompile_scripts", .label = "Recompile Scripts", .icon = ICON_MDI_REFRESH, .category = "Project", .is_enabled = is_editing, .run = [this] {
    if (sbx::core::engine::get_module<sbx::scripting::scripting_module>().recompile_scripts()) {
      _state.request_dirty_refresh();
    }
  }});
  actions.add(editor_action{.id = "project.bake_reflection_probes", .label = "Bake All Reflection Probes", .icon = ICON_MDI_MIRROR, .category = "Project", .is_enabled = [] { return !sbx::core::engine::get_module<sbx::render::scene_renderer_module>().is_capturing(); }, .run = [this, active_scene] { bake_all_reflection_probes(_state, active_scene()); }});
  actions.add(editor_action{.id = "editor.reset_layout", .label = "Reset Layout", .icon = ICON_MDI_VIEW_DASHBOARD_OUTLINE, .category = "Editor", .run = [this] { _reset_layout = true; }});
  actions.add(editor_action{.id = "editor.preferences", .label = "Preferences", .icon = ICON_MDI_TUNE, .category = "Editor", .run = [this] { _preferences_panel->is_open = true; }});
  actions.add(editor_action{.id = "editor.command_palette", .label = "Command Palette", .icon = ICON_MDI_CONSOLE_LINE, .category = "Editor", .default_chords = {ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P}, .run = [this] { _open_command_palette = true; }});

  // Asset Browser: polled by the panel.
  actions.add(editor_action{.id = "assets.rename", .label = "Rename Asset", .icon = ICON_MDI_PENCIL, .category = "Assets", .default_chords = {ImGuiKey_F2}, .context = action_context::asset_browser});
  actions.add(editor_action{.id = "assets.delete", .label = "Delete Asset", .icon = ICON_MDI_DELETE, .category = "Assets", .default_chords = {ImGuiKey_Delete}, .context = action_context::asset_browser});

  // The user's remapped shortcuts.
  auto overrides = std::unordered_map<std::string, std::vector<ImGuiKeyChord>>{};

  for (const auto& [id, names] : sbx::core::engine::get_module<editor::editor_module>().preferences().shortcut_overrides) {
    auto chords = std::vector<ImGuiKeyChord>{};

    for (const auto& name : names) {
      if (const auto chord = parse_chord(name)) {
        chords.push_back(*chord);
      }
    }

    overrides.emplace(id, std::move(chords));
  }

  actions.set_overrides(std::move(overrides));
}

// Fuzzy match: every query character appears in order; lower is better (earlier and tighter matches first), nullopt for no match.
auto palette_score(std::string_view text, std::string_view query) -> std::optional<std::size_t> {
  const auto lower = [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); };

  auto score = std::size_t{0u};
  auto position = std::size_t{0u};

  for (const auto character : query) {
    if (character == ' ') {
      continue;
    }

    const auto start = position;

    while (position < text.size() && lower(text[position]) != lower(character)) {
      ++position;
    }

    if (position == text.size()) {
      return std::nullopt;
    }

    score += position - start;
    ++position;
  }

  return score;
}

auto editor_ui_layer::_draw_command_palette() -> void {
  auto& actions = _state.actions;

  if (std::exchange(_open_command_palette, false)) {
    _palette_query.clear();
    _palette_index = 0u;
    ImGui::OpenPopup("##command_palette");
  }

  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.15f}, ImGuiCond_Always, ImVec2{0.5f, 0.0f});
  ImGui::SetNextWindowSize(ImVec2{560.0f, 0.0f});

  if (!ImGui::BeginPopupModal("##command_palette", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }

  ImGui::SetNextItemWidth(-1.0f);

  if (draw_text_field_with_hint("##command_palette_query", ICON_MDI_MAGNIFY " Type a command", _palette_query)) {
    _palette_index = 0u;
  }

  // Enabled actions only; with no query, the recent ones first.
  struct match {
    std::size_t score;
    sbx::memory::observer_ptr<const editor_action> action;
  }; // struct match

  auto matches = std::vector<match>{};

  for (const auto& action : actions.actions()) {
    if (!actions.is_enabled(action)) {
      continue;
    }

    const auto text = fmt::format("{}: {}", action.category, action.label);

    if (const auto score = palette_score(text, _palette_query)) {
      const auto recent = std::ranges::find(actions.recent(), action.id);
      const auto recency = _palette_query.empty() && recent != actions.recent().end() ? static_cast<std::size_t>(recent - actions.recent().begin()) : actions.recent().size();

      matches.push_back(match{_palette_query.empty() ? recency : *score, sbx::memory::make_observer(action)});
    }
  }

  std::ranges::stable_sort(matches, {}, &match::score);

  if (!matches.empty()) {
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      _palette_index = std::min(_palette_index + 1u, matches.size() - 1u);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && _palette_index > 0u) {
      --_palette_index;
    }

    _palette_index = std::min(_palette_index, matches.size() - 1u);
  }

  auto chosen = std::optional<std::string>{};

  if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
    if (!matches.empty()) {
      chosen = matches[_palette_index].action->id;
    }
  }

  ImGui::BeginChild("##command_palette_results", ImVec2{0.0f, std::min(static_cast<std::float_t>(matches.size()), 12.0f) * ImGui::GetFrameHeightWithSpacing()});

  for (auto index = std::size_t{0u}; index < matches.size(); ++index) {
    const auto& action = *matches[index].action;
    const auto shortcut = actions.shortcut_text(action.id);

    ImGui::PushID(static_cast<int>(index));

    if (ImGui::Selectable(fmt::format("{} {}: {}", action.icon, action.category, action.label).c_str(), index == _palette_index)) {
      chosen = action.id;
    }

    if (index == _palette_index && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
      ImGui::SetScrollHereY();
    }

    if (!shortcut.empty()) {
      ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(shortcut.c_str()).x);
      ImGui::TextDisabled("%s", shortcut.c_str());
    }

    ImGui::PopID();
  }

  if (matches.empty()) {
    ImGui::TextDisabled("No matching command.");
  }

  ImGui::EndChild();

  if (chosen || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();

  // Run after the palette closed, so an action can open its own popup.
  if (chosen) {
    actions.run(*chosen);
  }
}

auto editor_ui_layer::build() -> void {
  ImGuizmo::BeginFrame();

  // The interface scale changed (preference or monitor): the default style, scaled, with fonts scaled to match (ImGui 1.92 scales fonts at draw time).
  if (const auto scale = effective_interface_scale(); scale != _applied_interface_scale) {
    sbx::core::engine::get_module<sbx::render::ui_module>().apply_default_style();

    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;

    _applied_interface_scale = scale;
  }

  _handle_shortcuts();

  {
    SBX_PROFILE_SCOPE("editor_ui_layer::draw_dockspace");
    _draw_dockspace();
  }

  {
    SBX_PROFILE_SCOPE("editor_ui_layer::asset_watcher");
    _asset_watcher.update(_state);
  }

  // Waits out Play: swapping the game assembly would kill the running scripts.
  if (_state.scripts_changed && !sbx::core::engine::get_module<sbx::scenes::scenes_module>().is_simulating()) {
    _state.scripts_changed = false;

    if (sbx::core::engine::get_module<sbx::scripting::scripting_module>().recompile_scripts()) {
      _state.request_dirty_refresh();
    }
  }

  _handle_scene_file_request();

  {
    SBX_PROFILE_SCOPE("editor_ui_layer::refresh_dirty_scenes");
    _refresh_dirty_scenes();
  }

  {
    SBX_PROFILE_SCOPE("editor_ui_layer::thumbnails");
    _thumbnails.update();
  }

  _viewport_is_hovered = draw_viewport_window(_state, _sampler);

  for (auto& panel : _panels) {
    panel->draw(_state);
  }

  _draw_command_palette();

  if (sbx::core::engine::get_module<editor::editor_module>().play_state() == editor::play_state::edit) {
    update_selection_follow(_state, sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene());
  }

  // build() runs before the scene renderer's prepare(), so the outline follows the selection the same frame. Like Hazel, only while editing.
  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  const auto is_editing = sbx::core::engine::get_module<editor::editor_module>().play_state() == editor::play_state::edit;

  if (const auto* selection = std::get_if<node_selection>(&_state.current_selection); selection != nullptr && is_editing) {
    scene_renderer_module.set_selected_nodes(selection->ids);
  } else {
    scene_renderer_module.set_selected_nodes({});
  }
}

auto editor_ui_layer::_upload_fonts() -> void {
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ui_module.add_default_fonts(16.0f);
}

auto editor_ui_layer::_create_panels() -> void {
  _panels.push_back(std::make_unique<hierarchy_panel>());
  _panels.push_back(std::make_unique<inspector_panel>());
  _panels.push_back(std::make_unique<asset_browser_panel>());
  _panels.push_back(std::make_unique<logger_panel>());
  _panels.push_back(std::make_unique<animation_graph_panel>()); // on-demand panels below aren't in the default dock layout
  _panels.push_back(std::make_unique<shader_graph_panel>());
  _panels.push_back(std::make_unique<statistics_panel>());
  _panels.push_back(std::make_unique<import_window>());

  auto project_settings = std::make_unique<project_settings_panel>();
  _project_settings_panel = project_settings.get();
  _panels.push_back(std::move(project_settings));

  auto preferences = std::make_unique<preferences_panel>();
  _preferences_panel = preferences.get();
  _panels.push_back(std::move(preferences));

  auto history = std::make_unique<history_panel>();
  _history_panel = sbx::memory::make_observer(*history);
  _panels.push_back(std::move(history));

  auto navigation = std::make_unique<navigation_panel>();
  _navigation_panel = navigation.get();
  _panels.push_back(std::move(navigation));

  auto scene_renderer = std::make_unique<scene_renderer_panel>();
  _scene_renderer_panel = scene_renderer.get();
  _panels.push_back(std::move(scene_renderer));
}

auto editor_ui_layer::_draw_dockspace() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  if (_state.open_scene_request.has_value()) {
    const auto request = *_state.open_scene_request;
    _state.open_scene_request.reset();

    if (request.additive) {
      open_scene_additive(request.path);
    } else {
      open_scene(request.path);
    }
  }

  auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

  auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

  ImGui::Begin("##dockspace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  const auto dockspace_id = ImGui::GetID("editor_dockspace");

  // No saved layout for this dockspace yet, or Editor > Reset Layout: build the default.
  if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || std::exchange(_reset_layout, false)) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    // The right column splits off first at full height; the rest splits into a bottom strip (Asset Browser, Console) and a top strip (Hierarchy, Viewport).
    auto remaining = dockspace_id;
    auto right = ImGuiID{};
    auto bottom = ImGuiID{};
    auto left = ImGuiID{};
    auto center = ImGuiID{};

    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.20f, &right, &remaining);
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Down, 0.25f, &bottom, &remaining);
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.25f, &left, &center);

    // Side by side: Asset Browser 45%, Console 55%.
    auto bottom_left = ImGuiID{};
    auto bottom_right = ImGuiID{};

    ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.45f, &bottom_left, &bottom_right);

    if (auto* center_node = ImGui::DockBuilderGetNode(center)) {
      center_node->SetLocalFlags(center_node->LocalFlags | ImGuiDockNodeFlags_CentralNode);
    }

    // Each panel's own window_name constant, so renames can't desync the layout.
    ImGui::DockBuilderDockWindow(hierarchy_panel::window_name, left);
    ImGui::DockBuilderDockWindow(viewport_window_name, center);
    ImGui::DockBuilderDockWindow(asset_browser_panel::window_name, bottom_left);
    ImGui::DockBuilderDockWindow(logger_panel::window_name, bottom_right);
    ImGui::DockBuilderDockWindow(inspector_panel::window_name, right);
    ImGui::DockBuilderDockWindow(statistics_panel::window_name, right);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Reserves its strip before DockSpace() fills the rest.
  _draw_toolbar();

  ImGui::DockSpace(dockspace_id, ImVec2{0.0f, 0.0f});

  if (ImGui::BeginMenuBar()) {
    auto& actions = _state.actions;

    // Grouped like Godot's editor: Scene, Edit, Project, Debug, Editor, Window.
    if (ImGui::BeginMenu("Scene")) {
      actions.menu_item("scene.new");
      actions.menu_item("scene.open");
      actions.menu_item("scene.open_additive");

      auto& recent_scenes = editor_module.preferences().recent_scenes;

      // Scenes deleted or moved since drop out of the list.
      const auto assets_directory = sbx::core::engine::project().assets_directory();

      if (std::erase_if(recent_scenes, [&](const std::string& path) { return !std::filesystem::exists(assets_directory / path); }) > 0u) {
        editor_module.save_preferences();
      }

      if (ImGui::BeginMenuEx("Open Recent", ICON_MDI_HISTORY, !recent_scenes.empty() && editor_module.play_state() == editor::play_state::edit)) {
        auto chosen = std::optional<std::filesystem::path>{};

        for (const auto& path : recent_scenes) {
          if (ImGui::MenuItem(path.c_str())) {
            chosen = path;
          }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Clear Recent")) {
          recent_scenes.clear();
          editor_module.save_preferences();
        }

        ImGui::EndMenu();

        if (chosen) {
          open_scene(*chosen);
        }
      }

      ImGui::Separator();

      actions.menu_item("scene.save");
      actions.menu_item("scene.save_as");
      actions.menu_item("scene.save_all");

      ImGui::Separator();

      actions.menu_item("scene.quit");

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      actions.menu_item("edit.undo");
      actions.menu_item("edit.redo");
      actions.menu_item("edit.history");

      ImGui::Separator();

      actions.menu_item("edit.copy");
      actions.menu_item("edit.paste");
      actions.menu_item("edit.duplicate");
      actions.menu_item("edit.rename");
      actions.menu_item("edit.delete");

      ImGui::Separator();

      actions.menu_item("edit.add_child");
      actions.menu_item("edit.select_all");
      actions.menu_item("edit.deselect");

      ImGui::Separator();

      actions.menu_item("edit.focus_selection");
      actions.menu_item("edit.align_transform_with_view");
      actions.menu_item("edit.align_rotation_with_view");
      actions.menu_item("edit.snap_to_floor");

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project")) {
      actions.menu_item("project.settings");

      ImGui::Separator();

      actions.menu_item("project.recompile_scripts");
      actions.menu_item("project.bake_reflection_probes");

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
      actions.menu_item("debug.colliders");
      actions.menu_item("debug.broadphase");
      actions.menu_item("debug.contacts");

      ImGui::Separator();

      actions.menu_item("debug.navmesh");
      actions.menu_item("debug.nav_agents");

      ImGui::Separator();

      actions.menu_item("debug.shadow_cascades");
      actions.menu_item("view.wireframe");
      actions.menu_item("view.grid");

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Editor")) {
      actions.menu_item("editor.command_palette");
      actions.menu_item("editor.preferences");

      ImGui::Separator();

      actions.menu_item("editor.reset_layout");

      ImGui::EndMenu();
    }

    // Docked panels are always there, so they're brought to front; the others open and close.
    if (ImGui::BeginMenu("Window")) {
      const auto focus_item = [](const char* label, const char* icon, const char* window_name) {
        if (ImGui::MenuItemEx(label, icon)) {
          ImGui::SetWindowFocus(window_name);
        }
      };

      focus_item("Viewport", ICON_MDI_GAMEPAD_VARIANT, viewport_window_name);
      focus_item("Hierarchy", ICON_MDI_FILE_TREE, hierarchy_panel::window_name);
      focus_item("Inspector", ICON_MDI_INFORMATION, inspector_panel::window_name);
      focus_item("Asset Browser", ICON_MDI_FOLDER_MULTIPLE_IMAGE, asset_browser_panel::window_name);
      focus_item("Console", ICON_MDI_CONSOLE, logger_panel::window_name);
      focus_item("Statistics", ICON_MDI_CHART_BAR, statistics_panel::window_name);

      ImGui::Separator();

      const auto toggle_item = [](const char* label, const char* icon, bool& is_open) {
        if (ImGui::MenuItemEx(label, icon, nullptr, is_open)) {
          is_open = !is_open;
        }
      };

      toggle_item("History", ICON_MDI_HISTORY, _history_panel->is_open);
      toggle_item("Navigation", ICON_MDI_MAP_MARKER_PATH, _navigation_panel->is_open);
      toggle_item("Renderer", ICON_MDI_MONITOR, _scene_renderer_panel->is_open);

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  _draw_save_as_dialog();
  _draw_open_scene_dialog();
  _draw_unsaved_changes_dialog();

  ImGui::End();
}

auto editor_ui_layer::_draw_toolbar() -> void {
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto state = editor_module.play_state();
  const auto is_paused = state == editor::play_state::paused;

  constexpr auto button_size = ImVec2{28.0f, 28.0f};
  constexpr auto button_count = 3;
  const auto spacing = ImGui::GetStyle().ItemSpacing.x;
  const auto group_width = button_count * button_size.x + (button_count - 1) * spacing;

  // A flat strip under the menu bar, with no rounding or scrollbar.
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

  ImGui::BeginChild("##toolbar", ImVec2{0.0f, button_size.y + 2.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  ImGui::SetCursorPos(ImVec2{
    (ImGui::GetContentRegionAvail().x - group_width) * 0.5f,
    (ImGui::GetWindowHeight() - button_size.y) * 0.5f
  });

  ImGui::BeginGroup();

  // Play, Pause and Stop run their actions, so the buttons, the menus and F5/F7/F8 behave the same. Play is tinted while playing, Pause while paused.
  const auto play_button = [this, &button_size](std::string_view id, bool is_tinted) {
    auto& actions = _state.actions;
    const auto action = actions.find(id);

    if (is_tinted) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    ImGui::BeginDisabled(!actions.is_enabled(*action));

    if (ImGui::Button(action->icon, button_size)) {
      actions.run(id);
    }

    ImGui::EndDisabled();

    if (is_tinted) {
      ImGui::PopStyleColor();
    }

    actions.item_tooltip(id);
  };

  play_button("play.play", state == editor::play_state::playing);
  ImGui::SameLine();
  play_button("play.pause", is_paused);
  ImGui::SameLine();
  play_button("play.stop", false);

  ImGui::EndGroup();
  ImGui::EndChild();

  ImGui::PopStyleVar(2);
}

} // namespace editor
