// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
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
#include <editor/panels/scene_renderer_panel.hpp>
#include <editor/panels/project_settings_panel.hpp>
#include <editor/panels/preferences_panel.hpp>

#include <editor/widgets/layer_fields.hpp>

#include <editor/viewport_window.hpp>

#include <editor/editor_module.hpp>
#include <editor/node_actions.hpp>

#include <editor/commands/scene_commands.hpp>

#include <libsbx/core/engine.hpp>

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

  _create_panels();
}

auto editor_ui_layer::_handle_shortcuts() -> void {
  if (ImGui::GetIO().WantTextInput) {
    return;
  }

  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
  const auto is_editing = sbx::core::engine::get_module<editor::editor_module>().play_state() == editor::play_state::edit;

  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) {
    _state.undo(scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
    _state.redo(scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) {
    copy_selection(_state, scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
    paste_clipboard(_state, scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) {
    duplicate_selection(_state, scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiKey_Delete)) {
    delete_selection(_state, scene);
  } else if (ImGui::IsKeyChordPressed(ImGuiKey_Escape) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
    _state.clear_selection();
  } else if (is_editing && ImGui::IsKeyChordPressed(ImGuiKey_F)) {
    focus_selection(_state, scene);
  } else if (is_editing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
    _save();
  } else if (is_editing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) {
    _open_save_as_dialog();
  } else if (is_editing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
    new_scene();
  } else if (is_editing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) {
    _open_open_scene_dialog();
  }
}

auto editor_ui_layer::_save() -> void {
  if (_scene_path.empty()) {
    _open_save_as_dialog();
  } else {
    _save_scene(_scene_path);
  }
}

auto editor_ui_layer::build() -> void {
  ImGuizmo::BeginFrame();

  _handle_shortcuts();

  _draw_dockspace();

  _viewport_is_hovered = draw_viewport_window(_state, _sampler);

  for (auto& panel : _panels) {
    panel->draw(_state);
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

  auto project_settings = std::make_unique<project_settings_panel>();
  _project_settings_panel = project_settings.get();
  _panels.push_back(std::move(project_settings));

  auto preferences = std::make_unique<preferences_panel>();
  _preferences_panel = preferences.get();
  _panels.push_back(std::move(preferences));

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

    open_scene(request.path);
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

  if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
    // No saved layout for this dockspace yet: build the default once.
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
    if (ImGui::BeginMenu("File")) {
      ImGui::BeginDisabled(editor_module.play_state() != editor::play_state::edit);

      if (ImGui::MenuItem(ICON_MDI_FILE_PLUS " New Scene", "Ctrl+N")) {
        new_scene();
      }

      if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open Scene...", "Ctrl+O")) {
        _open_open_scene_dialog();
      }

      ImGui::EndDisabled();

      ImGui::Separator();

      if (ImGui::MenuItem("Quit")) {
        request_quit();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      ImGui::BeginDisabled(!_state.can_undo());

      if (ImGui::MenuItem(fmt::format(ICON_MDI_UNDO " Undo {}", _state.undo_label()).c_str(), "Ctrl+Z")) {
        _state.undo(scenes_module.active_scene());
      }

      ImGui::EndDisabled();

      ImGui::BeginDisabled(!_state.can_redo());

      if (ImGui::MenuItem(fmt::format(ICON_MDI_REDO " Redo {}", _state.redo_label()).c_str(), "Ctrl+Y")) {
        _state.redo(scenes_module.active_scene());
      }

      ImGui::EndDisabled();

      ImGui::Separator();

      const auto has_node_selection = _state.selected_node_count() > 0u;

      if (ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy", "Ctrl+C", false, has_node_selection)) {
        copy_selection(_state, scenes_module.active_scene());
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_PASTE " Paste", "Ctrl+V", false, !_state.node_clipboard.empty())) {
        paste_clipboard(_state, scenes_module.active_scene());
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_DUPLICATE " Duplicate", "Ctrl+D", false, has_node_selection)) {
        duplicate_selection(_state, scenes_module.active_scene());
      }

      if (ImGui::MenuItem(ICON_MDI_DELETE " Delete", "Delete", false, has_node_selection)) {
        delete_selection(_state, scenes_module.active_scene());
      }

      ImGui::Separator();

      if (ImGui::MenuItem(ICON_MDI_CROSSHAIRS_GPS " Focus Selection", "F", false, has_node_selection && editor_module.play_state() == editor::play_state::edit)) {
        focus_selection(_state, scenes_module.active_scene());
      }

      ImGui::Separator();

      if (ImGui::MenuItem(project_settings_panel::window_name)) {
        _project_settings_panel->is_open = true;
      }

      if (ImGui::MenuItem(preferences_panel::window_name)) {
        _preferences_panel->is_open = true;
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
      if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
        auto command = std::make_unique<create_node_command>();
        auto* created = command.get();

        _state.push_command(scenes_module.active_scene(), std::move(command));
        _state.select_node(scenes_module.active_scene().find(created->id()));
      }

      ImGui::Separator();

      ImGui::BeginDisabled(editor_module.play_state() != editor::play_state::edit);

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save", "Ctrl+S")) {
        _save();
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_EDIT " Save As...", "Ctrl+Shift+S")) {
        _open_save_as_dialog();
      }

      ImGui::EndDisabled();

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scripting")) {
      ImGui::BeginDisabled(editor_module.play_state() != editor::play_state::edit);

      if (ImGui::MenuItem(ICON_MDI_REFRESH " Recompile All")) {
        sbx::core::engine::get_module<sbx::scripting::scripting_module>().recompile_scripts();
      }

      ImGui::EndDisabled();

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();

      auto flags = physics_module.debug_draw_flags();
      auto changed = false;

      changed |= ImGui::MenuItem("Physics Colliders", nullptr, &flags.colliders);
      changed |= ImGui::MenuItem("Physics Broadphase", nullptr, &flags.broadphase);
      changed |= ImGui::MenuItem("Physics Contacts", nullptr, &flags.contacts);
      changed |= ImGui::MenuItem("Navmesh", nullptr, &flags.navmesh);
      changed |= ImGui::MenuItem("Nav Agents", nullptr, &flags.nav_agents);

      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

      auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
      auto grid_enabled = scene_renderer_module.grid_enabled();

      changed |= ImGui::MenuItem("Show Grid", nullptr, &grid_enabled);

      scene_renderer_module.set_grid_enabled(grid_enabled);

      auto wireframe_enabled = scene_renderer_module.wireframe_enabled();

      changed |= ImGui::MenuItem("Wireframe", nullptr, &wireframe_enabled);

      scene_renderer_module.set_wireframe_enabled(wireframe_enabled);

      auto shadow_cascade_debug_enabled = scene_renderer_module.shadow_cascade_debug_enabled();

      changed |= ImGui::MenuItem("Shadow Cascades", nullptr, &shadow_cascade_debug_enabled);

      scene_renderer_module.set_shadow_cascade_debug_enabled(shadow_cascade_debug_enabled);

      if (changed) {
        physics_module.set_debug_draw_flags(flags);
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem(navigation_panel::window_name, nullptr, &_navigation_panel->is_open);
      ImGui::MenuItem(scene_renderer_panel::window_name, nullptr, &_scene_renderer_panel->is_open);

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

  // Play starts a session from Edit or resumes a paused one; tinted while playing.
  {
    const auto can_play = state != editor::play_state::playing;

    if (state == editor::play_state::playing) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    ImGui::BeginDisabled(!can_play);

    if (ImGui::Button(ICON_MDI_PLAY, button_size)) {
      if (state == editor::play_state::edit) {
        _state.clear_selection();
        editor_module.enter_play_mode();
      } else {
        editor_module.toggle_pause();
      }
    }

    ImGui::EndDisabled();

    if (state == editor::play_state::playing) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(is_paused ? "Resume" : "Play");
    }
  }

  ImGui::SameLine();

  // Pause: only while playing; tinted while paused.
  {
    if (is_paused) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    ImGui::BeginDisabled(state != editor::play_state::playing);

    if (ImGui::Button(ICON_MDI_PAUSE, button_size)) {
      editor_module.toggle_pause();
    }

    ImGui::EndDisabled();

    if (is_paused) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Pause");
    }
  }

  ImGui::SameLine();

  // Stop: only while a session exists.
  {
    ImGui::BeginDisabled(state == editor::play_state::edit);

    if (ImGui::Button(ICON_MDI_STOP, button_size)) {
      _state.clear_selection();
      editor_module.exit_play_mode();
    }

    ImGui::EndDisabled();

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Stop");
    }
  }

  ImGui::EndGroup();
  ImGui::EndChild();

  ImGui::PopStyleVar(2);
}

} // namespace editor
