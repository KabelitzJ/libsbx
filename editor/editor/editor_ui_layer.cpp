// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <string>

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder* — see _draw_dockspace's first-run default layout.
#include <ImGuizmo.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/asset_browser_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/logger_panel.hpp>
#include <editor/panels/properties_panel.hpp>

#include <editor/viewport_gizmo.hpp>
#include <editor/viewport_picking.hpp>

#include <editor/editor_module.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

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

  // Shared with launcher (see ui_system::apply_default_style) so both windows look consistent
  // rather than each rolling its own theme.
  sbx::core::engine::get_module<sbx::render::ui_module>().apply_default_style();

  _create_panels();
}

auto editor_ui_layer::build() -> void {
  // Not owned by ui_system (ImGuizmo is editor-only), so it's this layer's job to prime it — must
  // run before any ImGuizmo:: call below.
  ImGuizmo::BeginFrame();

  _draw_dockspace();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(viewport_window_name, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // ImGui::PopStyleVar();

  _viewport_is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  const auto final_image = scene_renderer_module.final_image();

  if (final_image.is_valid() && available.x > 0.0f && available.y > 0.0f) {
    scene_renderer_module.set_viewport_extent(sbx::math::vector2u{width, height});

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();

    const auto texture_id = ui_module.texture_id(registry.get<sbx::graphics::image>(final_image).view(), _sampler);

    ImGui::Image(texture_id, available);

    // Captured before the gizmo call below, since ImGuizmo's own widgets can disturb ImGui's
    // "last item" tracking that IsItemClicked/GetItemRectMin rely on.
    const auto image_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const auto image_origin = ImGui::GetItemRectMin();

    const auto gizmo_active = draw_viewport_gizmo(_state, image_origin, available);
    const auto toolbar_active = draw_gizmo_toolbar(_state, image_origin);
    const auto view_gizmo_active = draw_view_gizmo(image_origin, available);
    const auto icons_active = draw_node_icons(_state, image_origin, available, gizmo_active);

    // Left-click picks the node under the cursor, unless it landed on the gizmo, its toolbar, the
    // view-orientation cube, or a light/camera icon.
    if (image_clicked && !gizmo_active && !toolbar_active && !view_gizmo_active && !icons_active) {
      const auto mouse_position = ImGui::GetMousePos();

      pick_node_at_viewport_position(_state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::Begin(stats_window_name);
  ImGui::Text("%.1f FPS (%.3f ms)", static_cast<double>(ImGui::GetIO().Framerate), 1000.0 / static_cast<double>(ImGui::GetIO().Framerate));
  ImGui::End();

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
  _panels.push_back(std::make_unique<properties_panel>());
  _panels.push_back(std::make_unique<asset_browser_panel>());
  _panels.push_back(std::make_unique<logger_panel>());
}

auto editor_ui_layer::_draw_dockspace() -> void {
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
    // First time this project has ever opened (no imgui.ini yet, or it had nothing for this
    // dockspace) — lay out a sane default instead of leaving every panel undocked and stacked
    // in one corner. Only runs once per node id: after this, or after a saved layout is loaded
    // from disk, the node already exists and this is skipped every subsequent frame.
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    // Right column (Properties + Stats) split off first, full height; then the remainder split
    // into a bottom strip (Asset Browser + Console) and a top strip, which is itself split into
    // Hierarchy (left) and Viewport (center, marked as the central node) — same shape/order as
    // the reference layout this was matched against, not just the same end result.
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
    ImGui::DockBuilderDockWindow(properties_panel::window_name, right);
    ImGui::DockBuilderDockWindow(stats_window_name, right);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Reserves its own strip of vertical space (ordinary content, drawn top-down) before DockSpace()
  // below claims whatever's left via its ImVec2{0,0} "fill remaining" size — mirroring how the
  // real menu bar's height is already excluded from that same remaining region via the
  // ImGuiWindowFlags_MenuBar flag on this window.
  _draw_toolbar();

  ImGui::DockSpace(dockspace_id, ImVec2{0.0f, 0.0f});

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Quit")) {
        request_quit();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
      if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto& scene = scenes_module.active_scene();

        _state.select_node(scene.create_node());
      }

      ImGui::Separator();

      auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

      // Saving while playing would write play-mutated (and script-stripped, see
      // scripting::scripts) state over the user's file — block it, same as the disabled particle
      // transport buttons in properties_panel.cpp.
      ImGui::BeginDisabled(editor_module.play_state() != editor::play_state::edit);

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save")) {
        if (_scene_path.empty()) {
          std::strncpy(_save_as_buffer.data(), "scenes/new_scene.yaml", _save_as_buffer.size() - 1u);
          _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
          _show_save_as_dialog = true;
        } else {
          _save_scene(_scene_path);
        }
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_EDIT " Save As...")) {
        const auto& seed = _scene_path.empty() ? std::string{"scenes/new_scene.yaml"} : _scene_path.string();
        std::strncpy(_save_as_buffer.data(), seed.c_str(), _save_as_buffer.size() - 1u);
        _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
        _show_save_as_dialog = true;
      }

      ImGui::Separator();

      // Same Edit-mode-only guard as Save above: reloading the game assembly while something is
      // instantiate()'d from it (i.e. while playing/paused) isn't safe — see
      // scripting_module::recompile_scripts's doc comment.
      if (ImGui::MenuItem(ICON_MDI_REFRESH " Recompile Scripts")) {
        sbx::core::engine::get_module<sbx::scripting::scripting_module>().recompile_scripts();
      }

      ImGui::EndDisabled();

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  _draw_save_as_dialog();
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

auto editor_ui_layer::request_quit() -> void {
  if (_is_scene_dirty()) {
    _show_unsaved_changes_dialog = true;
  } else {
    sbx::core::engine::quit();
  }
}

auto editor_ui_layer::_save_scene(const std::filesystem::path& path) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  sbx::scenes::scene_serializer::save(scenes_module.active_scene(), path);

  _scene_path = path;
}

auto editor_ui_layer::_is_scene_dirty() -> bool {
  if (_scene_path.empty()) {
    return true; // never saved — anything at all counts as unsaved
  }

  auto& project = sbx::core::engine::project();
  auto file = std::ifstream{project.assets_directory() / _scene_path, std::ios::binary};

  if (!file) {
    return true; // no file at that path (yet)
  }

  const auto on_disk = std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  return on_disk != sbx::scenes::scene_serializer::serialize(scenes_module.active_scene());
}

auto editor_ui_layer::_draw_save_as_dialog() -> void {
  if (_show_save_as_dialog) {
    ImGui::OpenPopup("Save Scene As");
    _show_save_as_dialog = false;
  }

  if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextDisabled("Relative to the project's assets directory.");
    ImGui::InputText("##save_as_path", _save_as_buffer.data(), _save_as_buffer.size());

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (_save_as_buffer[0] != '\0') {
        _save_scene(std::filesystem::path{_save_as_buffer.data()});

        if (_quit_after_save_as) {
          _quit_after_save_as = false;
          sbx::core::engine::quit();
        }

        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

auto editor_ui_layer::_draw_unsaved_changes_dialog() -> void {
  if (_show_unsaved_changes_dialog) {
    ImGui::OpenPopup("Unsaved Changes");
    _show_unsaved_changes_dialog = false;
  }

  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(ICON_MDI_CONTENT_SAVE_ALERT " The current scene has unsaved changes.");

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (_scene_path.empty()) {
        std::strncpy(_save_as_buffer.data(), "scenes/new_scene.yaml", _save_as_buffer.size() - 1u);
        _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
        _show_save_as_dialog = true;
        _quit_after_save_as = true;
      } else {
        _save_scene(_scene_path);
        sbx::core::engine::quit();
      }

      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Don't Save")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
      sbx::core::engine::quit();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
