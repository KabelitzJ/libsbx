// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <string>

#include <imgui.h>
#include <ImGuizmo.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/asset_browser_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/logger_panel.hpp>
#include <editor/panels/properties_panel.hpp>

#include <editor/viewport_gizmo.hpp>
#include <editor/viewport_picking.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/render_module.hpp>

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
  _apply_style();
  _create_panels();
}

auto editor_ui_layer::build() -> void {
  // Not owned by ui_system (ImGuizmo is editor-only), so it's this layer's job to prime it — must
  // run before any ImGuizmo:: call below.
  ImGuizmo::BeginFrame();

  _draw_dockspace();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // ImGui::PopStyleVar();

  _viewport_is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  const auto final_image = render_module.final_image();

  if (final_image.is_valid() && available.x > 0.0f && available.y > 0.0f) {
    render_module.set_viewport_extent(sbx::math::vector2u{width, height});

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();

    const auto texture_id = render_module.ui().texture_id(registry.get<sbx::graphics::image>(final_image).view(), _sampler);

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
    // view-orientation cube, or a light/camera icon (right-drag is already the fly camera, so
    // there's no input conflict there either).
    if (image_clicked && !gizmo_active && !toolbar_active && !view_gizmo_active && !icons_active) {
      const auto mouse_position = ImGui::GetMousePos();

      pick_node_at_viewport_position(_state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::Begin("Stats");
  ImGui::Text("%.1f FPS (%.3f ms)", static_cast<double>(ImGui::GetIO().Framerate), 1000.0 / static_cast<double>(ImGui::GetIO().Framerate));
  ImGui::End();

  for (auto& panel : _panels) {
    panel->draw(_state);
  }
}

auto editor_ui_layer::_upload_fonts() -> void {
  // Roboto Regular + Material Design Icons (matches ICON_MIN_MDI/ICON_MAX_MDI in
  // <libsbx/render/ui/fonts/material_design_icons.hpp>, used throughout the panels below),
  // embedded in the engine itself — see ui_system::add_default_fonts.
  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  render_module.ui().add_default_fonts(16.0f);
}

auto editor_ui_layer::_apply_style() -> void {
  auto& style = ImGui::GetStyle();
  auto* colors = style.Colors;

  // Catppuccin Mocha Palette
  // --------------------------------------------------------
  const auto base       = ImVec4(0.117f, 0.117f, 0.172f, 1.0f); // #1e1e2e
  const auto mantle     = ImVec4(0.109f, 0.109f, 0.156f, 1.0f); // #181825
  const auto surface0   = ImVec4(0.200f, 0.207f, 0.286f, 1.0f); // #313244
  const auto surface1   = ImVec4(0.247f, 0.254f, 0.337f, 1.0f); // #3f4056
  const auto surface2   = ImVec4(0.290f, 0.301f, 0.388f, 1.0f); // #4a4d63
  const auto overlay0   = ImVec4(0.396f, 0.403f, 0.486f, 1.0f); // #65677c
  const auto overlay2   = ImVec4(0.576f, 0.584f, 0.654f, 1.0f); // #9399b2
  const auto text       = ImVec4(0.803f, 0.815f, 0.878f, 1.0f); // #cdd6f4
  const auto subtext0   = ImVec4(0.639f, 0.658f, 0.764f, 1.0f); // #a3a8c3
  const auto mauve      = ImVec4(0.796f, 0.698f, 0.972f, 1.0f); // #cba6f7
  const auto peach      = ImVec4(0.980f, 0.709f, 0.572f, 1.0f); // #fab387
  const auto yellow     = ImVec4(0.980f, 0.913f, 0.596f, 1.0f); // #f9e2af
  const auto green      = ImVec4(0.650f, 0.890f, 0.631f, 1.0f); // #a6e3a1
  const auto teal       = ImVec4(0.580f, 0.886f, 0.819f, 1.0f); // #94e2d5
  const auto sapphire   = ImVec4(0.458f, 0.784f, 0.878f, 1.0f); // #74c7ec
  const auto blue       = ImVec4(0.533f, 0.698f, 0.976f, 1.0f); // #89b4fa
  const auto lavender   = ImVec4(0.709f, 0.764f, 0.980f, 1.0f); // #b4befe

  // Main window and backgrounds
  colors[ImGuiCol_WindowBg]             = base;
  colors[ImGuiCol_ChildBg]              = base;
  colors[ImGuiCol_PopupBg]              = surface0;
  colors[ImGuiCol_Border]               = surface1;
  colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg]              = surface0;
  colors[ImGuiCol_FrameBgHovered]       = surface1;
  colors[ImGuiCol_FrameBgActive]        = surface2;
  colors[ImGuiCol_TitleBg]              = mantle;
  colors[ImGuiCol_TitleBgActive]        = surface0;
  colors[ImGuiCol_TitleBgCollapsed]     = mantle;
  colors[ImGuiCol_MenuBarBg]            = mantle;
  colors[ImGuiCol_ScrollbarBg]          = surface0;
  colors[ImGuiCol_ScrollbarGrab]        = surface2;
  colors[ImGuiCol_ScrollbarGrabHovered] = overlay0;
  colors[ImGuiCol_ScrollbarGrabActive]  = overlay2;
  colors[ImGuiCol_CheckMark]            = text;
  colors[ImGuiCol_SliderGrab]           = sapphire;
  colors[ImGuiCol_SliderGrabActive]     = blue;
  colors[ImGuiCol_Button]               = surface0;
  colors[ImGuiCol_ButtonHovered]        = surface1;
  colors[ImGuiCol_ButtonActive]         = surface2;
  colors[ImGuiCol_Header]               = surface0;
  colors[ImGuiCol_HeaderHovered]        = surface1;
  colors[ImGuiCol_HeaderActive]         = surface2;
  colors[ImGuiCol_Separator]            = surface1;
  colors[ImGuiCol_SeparatorHovered]     = mauve;
  colors[ImGuiCol_SeparatorActive]      = mauve;
  colors[ImGuiCol_ResizeGrip]           = surface2;
  colors[ImGuiCol_ResizeGripHovered]    = mauve;
  colors[ImGuiCol_ResizeGripActive]     = mauve;
  colors[ImGuiCol_Tab]                  = surface0;
  colors[ImGuiCol_TabHovered]           = surface2;
  colors[ImGuiCol_TabActive]            = surface1;
  colors[ImGuiCol_TabUnfocused]         = surface0;
  colors[ImGuiCol_TabUnfocusedActive]   = surface1;
  colors[ImGuiCol_DockingPreview]       = sapphire;
  colors[ImGuiCol_DockingEmptyBg]       = base;
  colors[ImGuiCol_PlotLines]            = blue;
  colors[ImGuiCol_PlotLinesHovered]     = peach;
  colors[ImGuiCol_PlotHistogram]        = teal;
  colors[ImGuiCol_PlotHistogramHovered] = green;
  colors[ImGuiCol_TableHeaderBg]        = surface0;
  colors[ImGuiCol_TableBorderStrong]    = surface1;
  colors[ImGuiCol_TableBorderLight]     = surface0;
  colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
  colors[ImGuiCol_TextSelectedBg]       = surface2;
  colors[ImGuiCol_DragDropTarget]       = yellow;
  colors[ImGuiCol_NavHighlight]         = lavender;
  colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
  colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
  colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
  colors[ImGuiCol_Text]                 = text;
  colors[ImGuiCol_TextDisabled]         = subtext0;

  // Rounded corners
  style.WindowRounding    = 6.0f;
  style.ChildRounding     = 6.0f;
  style.FrameRounding     = 4.0f;
  style.PopupRounding     = 4.0f;
  style.ScrollbarRounding = 9.0f;
  style.GrabRounding      = 4.0f;
  style.TabRounding       = 4.0f;

  // Padding and spacing
  style.WindowPadding     = ImVec2(8.0f, 8.0f);
  style.FramePadding      = ImVec2(5.0f, 3.0f);
  style.ItemSpacing       = ImVec2(8.0f, 4.0f);
  style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
  style.IndentSpacing     = 21.0f;
  style.ScrollbarSize     = 14.0f;
  style.GrabMinSize       = 10.0f;

  // Borders
  style.WindowBorderSize  = 1.0f;
  style.ChildBorderSize   = 1.0f;
  style.PopupBorderSize   = 1.0f;
  style.FrameBorderSize   = 0.0f;
  style.TabBorderSize     = 0.0f;

  // Go through every colour and convert it to linear
  // This is because ImGui uses linear colours but we are using sRGB
  // This is a simple approximation of the conversion
  for (auto i = 0; i < ImGuiCol_COUNT; ++i) {
    auto& color = style.Colors[i];
    color.x = color.x <= 0.04045f ? color.x / 12.92f : std::pow((color.x + 0.055f) / 1.055f, 2.4f);
    color.y = color.y <= 0.04045f ? color.y / 12.92f : std::pow((color.y + 0.055f) / 1.055f, 2.4f);
    color.z = color.z <= 0.04045f ? color.z / 12.92f : std::pow((color.z + 0.055f) / 1.055f, 2.4f);
  }
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

  ImGui::DockSpace(ImGui::GetID("editor_dockspace"), ImVec2{0.0f, 0.0f});

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

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  _draw_save_as_dialog();
  _draw_unsaved_changes_dialog();

  ImGui::End();
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
