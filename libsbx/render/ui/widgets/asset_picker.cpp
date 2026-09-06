// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/asset_picker.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

namespace sbx::render::widgets {

namespace {

[[nodiscard]] auto icon_for(asset_picker_kind kind) -> const char* {
  switch (kind) {
    case asset_picker_kind::texture: return ICON_MDI_IMAGE;
    case asset_picker_kind::mesh: return ICON_MDI_CUBE_OUTLINE;
    case asset_picker_kind::material: return ICON_MDI_PALETTE_SWATCH;
    case asset_picker_kind::particle_effect: return ICON_MDI_FIREWORK;
    case asset_picker_kind::animation_graph: return ICON_MDI_STATE_MACHINE;
  }

  return ICON_MDI_FILE_OUTLINE;
}

// Case-insensitive substring test for the filter box below.
[[nodiscard]] auto contains_ignore_case(std::string_view haystack, std::string_view needle) -> bool {
  const auto to_lower = [](std::string_view text) {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

[[nodiscard]] auto collect_matching_files(const std::filesystem::path& root, const std::vector<std::string>& extensions) -> std::vector<std::filesystem::path> {
  auto files = std::vector<std::filesystem::path>{};

  if (!std::filesystem::exists(root)) {
    return files;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
    if (!entry.is_directory() && std::ranges::find(extensions, entry.path().extension().string()) != extensions.end()) {
      files.push_back(entry.path());
    }
  }

  return files;
}

} // namespace

auto drag_payload_type_for(asset_picker_kind kind) -> const char* {
  switch (kind) {
    case asset_picker_kind::texture: return drag_drop_payload_texture;
    case asset_picker_kind::mesh: return drag_drop_payload_mesh;
    case asset_picker_kind::material: return drag_drop_payload_material;
    case asset_picker_kind::particle_effect: return drag_drop_payload_particle_effect;
    case asset_picker_kind::animation_graph: return drag_drop_payload_animation_graph;
  }

  return drag_drop_payload_texture;
}

auto draw_asset_picker(const char* popup_id, const asset_picker_item& current, const asset_picker_item& default_item, const asset_picker_options& options) -> asset_picker_result {
  auto result = asset_picker_result{};

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& project = core::engine::project();

  const auto has_current = !current.path.empty();

  ImGui::PushID(popup_id);
  ImGui::BeginGroup();

  constexpr auto button_size = ImVec2{28.0f, 28.0f};

  auto button_tile = asset_tile_desc{};
  button_tile.icon_glyph = icon_for(options.kind);
  button_tile.size = button_size;

  if (options.kind == asset_picker_kind::texture && has_current) {
    button_tile.is_texture_thumbnail = true;
    button_tile.texture = assets_module.load_texture(current.path, options.load_format);
  }

  const auto button_tile_result = draw_asset_tile("##button_tile", button_tile);

  // The closed button is a drop target too -- dragging a matching asset from the Asset Browser
  // straight onto the field works without ever opening the popup.
  if (ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload(drag_payload_type_for(options.kind))) {
      auto dropped = asset_drag_payload{};
      std::memcpy(&dropped, payload->Data, sizeof(dropped));

      result.changed = true;
      result.picked = asset_picker_item{dropped.id, std::filesystem::path{dropped.path}};
    }

    ImGui::EndDragDropTarget();
  }

  if (button_tile_result.clicked) {
    ImGui::OpenPopup(popup_id);
  }

  ImGui::SameLine();

  const auto label = has_current ? current.path.filename().string() : std::string{"(None)"};

  // A plain Button (auto-sized to its text) rather than a width-stretching Selectable -- a
  // Selectable with size.x == 0 fills the *entire* remaining window width, which would swallow
  // the edit button right after it (and anything the caller places after the whole picker, like
  // mesh_renderer's "Duplicate" button) into its own click area.
  if (ImGui::Button(label.c_str())) {
    ImGui::OpenPopup(popup_id);
  }

  if (has_current && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", current.path.string().c_str());
  }

  if (options.show_edit_button && has_current) {
    ImGui::SameLine();

    if (ImGui::Button(ICON_MDI_FILE_EDIT_OUTLINE)) {
      result.edit_requested = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Edit this asset");
    }
  }

  if (options.show_reveal_button && has_current) {
    ImGui::SameLine();

    if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE)) {
      result.reveal_requested = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show in Asset Browser");
    }
  }

  ImGui::EndGroup();

  // Fixed width so the popup doesn't reflow (and the filter box along with it) as filtering
  // changes which entries -- and therefore how wide the widest visible one is -- are shown.
  ImGui::SetNextWindowSize(ImVec2{340.0f, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup(popup_id)) {
    // Reset whenever a *different* picker's popup opens, so leftover text doesn't carry over.
    static auto filter_buffer = std::array<char, 128u>{};
    static auto last_popup_id = std::string{};

    if (last_popup_id != popup_id) {
      filter_buffer[0] = '\0';
      last_popup_id = popup_id;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", filter_buffer.data(), filter_buffer.size());

    if (options.allow_none && ImGui::MenuItem(ICON_MDI_CLOSE_CIRCLE_OUTLINE " (None)")) {
      result.changed = true;
      result.cleared = true;
    }

    if (!default_item.path.empty() && ImGui::MenuItem(ICON_MDI_RESTORE " Reset to Default")) {
      result.changed = true;
      result.reset_to_default = true;
      result.picked = default_item;
    }

    auto visible = std::vector<std::filesystem::path>{};

    for (const auto& file : collect_matching_files(project.assets_directory(), options.extensions)) {
      auto relative = std::filesystem::relative(file, project.assets_directory());

      if (filter_buffer[0] == '\0' || contains_ignore_case(relative.string(), filter_buffer.data())) {
        visible.push_back(std::move(relative));
      }
    }

    std::ranges::sort(visible);

    constexpr auto row_size = ImVec2{24.0f, 24.0f};
    const auto row_height = std::max(row_size.y, ImGui::GetTextLineHeight()) + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginChild("##asset_picker_list", ImVec2{0.0f, 260.0f})) {
      // Clipped so a large project only ever loads/thumbnails the rows actually on screen -- see
      // the "unbounded texture loads" risk this addresses in the implementation plan.
      auto clipper = ImGuiListClipper{};
      clipper.Begin(static_cast<std::int32_t>(visible.size()), row_height);

      while (clipper.Step()) {
        for (auto index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto& relative = visible[static_cast<std::size_t>(index)];
          const auto relative_string = relative.string();

          ImGui::PushID(index);

          auto row_tile = asset_tile_desc{};
          row_tile.icon_glyph = icon_for(options.kind);
          row_tile.size = row_size;

          if (options.kind == asset_picker_kind::texture) {
            row_tile.is_texture_thumbnail = true;
            row_tile.texture = assets_module.load_texture(relative, options.load_format);
          }

          const auto row_tile_result = draw_asset_tile("##row_tile", row_tile);

          ImGui::SameLine();

          const auto row_clicked = ImGui::Selectable(relative_string.c_str(), false, 0, ImVec2{0.0f, row_size.y});

          if (row_clicked || row_tile_result.clicked) {
            result.changed = true;
            result.picked = asset_picker_item{sbx::math::uuid::nil(), relative};
            ImGui::CloseCurrentPopup();
          }

          ImGui::PopID();
        }
      }
    }

    ImGui::EndChild();

    ImGui::EndPopup();
  }

  ImGui::PopID();

  return result;
}

} // namespace sbx::render::widgets
