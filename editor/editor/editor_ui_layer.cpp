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

#include <editor/widgets/layer_fields.hpp>

#include <editor/viewport_window.hpp>

#include <editor/editor_module.hpp>

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

auto editor_ui_layer::build() -> void {
  ImGuizmo::BeginFrame();

  if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      _state.undo(scenes_module.active_scene());
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      _state.redo(scenes_module.active_scene());
    }
  }

  _draw_dockspace();

  _viewport_is_hovered = draw_viewport_window(_state, _sampler);

  for (auto& panel : _panels) {
    panel->draw(_state);
  }
}

auto editor_ui_layer::_upload_fonts() -> void {
  // Roboto Regular + Material Design Icons, embedded in the engine — see ui_system::add_default_fonts.
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ui_module.add_default_fonts(16.0f);
}

auto editor_ui_layer::_create_panels() -> void {
  _panels.push_back(std::make_unique<hierarchy_panel>());
  _panels.push_back(std::make_unique<inspector_panel>());
  _panels.push_back(std::make_unique<asset_browser_panel>());
  _panels.push_back(std::make_unique<logger_panel>());
  _panels.push_back(std::make_unique<animation_graph_panel>()); // on-demand, not part of the default dock layout -- see its own doc comment
  _panels.push_back(std::make_unique<shader_graph_panel>()); // on-demand, same reasoning as animation_graph_panel above
  _panels.push_back(std::make_unique<statistics_panel>()); // always-open, replaces the old inline FPS-only Stats window

  auto navigation = std::make_unique<navigation_panel>();
  _navigation_panel = navigation.get();
  _panels.push_back(std::move(navigation)); // on-demand, same reasoning as animation_graph_panel above

  auto scene_renderer = std::make_unique<scene_renderer_panel>();
  _scene_renderer_panel = scene_renderer.get();
  _panels.push_back(std::move(scene_renderer)); // on-demand, same reasoning as animation_graph_panel above
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
    // No saved layout for this dockspace yet — lay out a sane default. Runs once per node id;
    // once it exists (here or from a loaded layout on disk), this is skipped every frame after.
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    // Right column (Properties + Stats) split off first, full height; remainder splits into a
    // bottom strip (Asset Browser + Console) and a top strip (Hierarchy left, Viewport center).
    auto remaining = dockspace_id;
    auto right = ImGuiID{};
    auto bottom = ImGuiID{};
    auto left = ImGuiID{};
    auto center = ImGuiID{};

    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.20f, &right, &remaining);
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Down, 0.25f, &bottom, &remaining);
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.25f, &left, &center);

    // Side by side, not tabbed: Asset Browser left (45%), Console right (55%).
    auto bottom_left = ImGuiID{};
    auto bottom_right = ImGuiID{};

    ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.45f, &bottom_left, &bottom_right);

    if (auto* center_node = ImGui::DockBuilderGetNode(center)) {
      center_node->SetLocalFlags(center_node->LocalFlags | ImGuiDockNodeFlags_CentralNode);
    }

    // Each name here is the same window_name constant its own panel's ImGui::Begin() uses (see
    // hierarchy_panel::window_name and friends) — never a re-typed literal, so a renamed panel
    // can't silently desync from this layout.
    ImGui::DockBuilderDockWindow(hierarchy_panel::window_name, left);
    ImGui::DockBuilderDockWindow(viewport_window_name, center);
    ImGui::DockBuilderDockWindow(asset_browser_panel::window_name, bottom_left);
    ImGui::DockBuilderDockWindow(logger_panel::window_name, bottom_right);
    ImGui::DockBuilderDockWindow(inspector_panel::window_name, right);
    ImGui::DockBuilderDockWindow(statistics_panel::window_name, right);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Reserves its own strip of vertical space before DockSpace() below claims whatever's left via
  // its ImVec2{0,0} "fill remaining" size, the same way the menu bar's height is excluded via
  // ImGuiWindowFlags_MenuBar on this window.
  _draw_toolbar();

  ImGui::DockSpace(dockspace_id, ImVec2{0.0f, 0.0f});

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      ImGui::BeginDisabled(editor_module.play_state() != editor::play_state::edit);

      if (ImGui::MenuItem(ICON_MDI_FILE_PLUS " New Scene")) {
        new_scene();
      }

      if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open Scene...")) {
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

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save")) {
        if (_scene_path.empty()) {
          _open_save_as_dialog();
        } else {
          _save_scene(_scene_path);
        }
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_EDIT " Save As...")) {
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
  _draw_edit_layers_popup();

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

  // Flat strip flush with the menu bar above it — no rounded box outline, no scrollbar (the group
  // is sized to fit exactly, but a stray sub-pixel overflow shouldn't ever spawn one).
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

  ImGui::BeginChild("##toolbar", ImVec2{0.0f, button_size.y + 2.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  ImGui::SetCursorPos(ImVec2{
    (ImGui::GetContentRegionAvail().x - group_width) * 0.5f,
    (ImGui::GetWindowHeight() - button_size.y) * 0.5f
  });

  ImGui::BeginGroup();

  // Play: starts a fresh session from edit, or resumes one that's paused. Disabled while already
  // playing; tinted while it's the state that's currently active (playing).
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

  // Pause: only meaningful while actively playing. Tinted while it's the current state (paused).
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

  // Stop: only meaningful once a play session (playing or paused) exists.
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

auto editor_ui_layer::_draw_edit_layers_popup() -> void {
  if (_state.open_edit_layers_popup_request) {
    ImGui::OpenPopup("Edit Layers");
    _state.open_edit_layers_popup_request = false;
  }

  // Fixed size (still resizable, just not AlwaysAutoResize) -- a scrollable list on the left and a
  // matrix that can grow wide on the right both want a stable frame to scroll inside of, not a
  // window that keeps refitting itself to whatever's currently visible.
  ImGui::SetNextWindowSize(ImVec2{760.0f, 520.0f}, ImGuiCond_Appearing);

  if (!ImGui::BeginPopupModal("Edit Layers", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    return;
  }

  auto& project = sbx::core::engine::project();

  if (ImGui::IsWindowAppearing()) {
    // Resync display order from whatever's actually named right now -- see _edit_layer_rows'
    // doc comment for why this only happens on open, never per-frame.
    _edit_layer_rows.clear();

    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      if (!project.layers()[index].empty()) {
        _edit_layer_rows.push_back(static_cast<std::uint8_t>(index));
      }
    }
  }

  const auto content_region = ImGui::GetContentRegionAvail();
  const auto list_width = content_region.x * 0.25f;
  const auto body_height = content_region.y - ImGui::GetFrameHeightWithSpacing();

  if (ImGui::BeginChild("##layer_list", ImVec2{list_width, body_height}, true)) {
    ImGui::TextUnformatted("Layers");
    ImGui::Separator();

    auto pending_remove = std::optional<std::uint8_t>{};

    for (const auto index : _edit_layer_rows) {
      ImGui::PushID(static_cast<int>(index));

      auto buffer = std::array<char, 64u>{};
      const auto& name = project.layers()[index];
      std::strncpy(buffer.data(), name.c_str(), buffer.size() - 1u);
      buffer[buffer.size() - 1u] = '\0';

      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);

      // Deliberately not gated on non-empty: an in-progress rename (select-all, retype) passes
      // through an empty string, and that must not delete the row out from under the user -- only
      // the "x" button (below) does that.
      if (ImGui::InputText("##name", buffer.data(), buffer.size())) {
        project.set_layer_name(static_cast<std::uint8_t>(index), std::string{buffer.data()});
        project.save();
      }

      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_CLOSE)) {
        pending_remove = static_cast<std::uint8_t>(index);
      }

      ImGui::PopID();
    }

    if (pending_remove) {
      project.set_layer_name(*pending_remove, std::string{});
      project.save();
      std::erase(_edit_layer_rows, *pending_remove);
    }

    ImGui::Spacing();

    const auto has_free_slot = _edit_layer_rows.size() < sbx::core::layer_count;

    ImGui::BeginDisabled(!has_free_slot);

    if (ImGui::Button(ICON_MDI_PLUS " Add Layer", ImVec2{-1.0f, 0.0f})) {
      for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
        if (project.layers()[index].empty()) {
          project.set_layer_name(static_cast<std::uint8_t>(index), "New Layer");
          project.save();
          _edit_layer_rows.push_back(static_cast<std::uint8_t>(index)); // always appended -- always the bottom row, regardless of which numeric slot it reused
          break;
        }
      }
    }

    ImGui::EndDisabled();
  }

  ImGui::EndChild();

  ImGui::SameLine();

  if (ImGui::BeginChild("##layer_matrix_pane", ImVec2{0.0f, body_height}, true)) {
    ImGui::TextUnformatted("Layer Collision Matrix");
    ImGui::TextDisabled("Unchecking a cell stops those two layers from physically colliding.");
    ImGui::Separator();

    auto named_indices = std::vector<std::uint8_t>{};

    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      if (!project.layers()[index].empty()) {
        named_indices.push_back(static_cast<std::uint8_t>(index));
      }
    }

    if (named_indices.empty()) {
      ImGui::TextDisabled("No named layers yet -- add one on the left.");
    } else {
      const auto column_count = static_cast<int>(named_indices.size()) + 1;
      const auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

      if (ImGui::BeginTable("##layer_matrix", column_count, table_flags, ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110.0f);

        for (const auto column_layer : named_indices) {
          ImGui::TableSetupColumn(project.layer_name(column_layer).c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        }

        ImGui::TableHeadersRow();

        for (const auto row_layer : named_indices) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(project.layer_name(row_layer).c_str());

          auto column_slot = 1;

          for (const auto column_layer : named_indices) {
            ImGui::TableSetColumnIndex(column_slot);
            ++column_slot;

            ImGui::PushID(static_cast<int>(row_layer) * static_cast<int>(sbx::core::layer_count) + static_cast<int>(column_layer));

            auto collide = project.layers_collide(row_layer, column_layer);

            if (ImGui::Checkbox("##cell", &collide)) {
              project.set_layers_collide(row_layer, column_layer, collide);
              project.save();
            }

            ImGui::PopID();
          }
        }

        ImGui::EndTable();
      }
    }
  }

  ImGui::EndChild();

  if (ImGui::Button("Close")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

} // namespace editor
