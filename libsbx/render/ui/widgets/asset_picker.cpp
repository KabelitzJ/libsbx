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

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/primitive_meshes.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

namespace sbx::render {

[[nodiscard]] auto icon_for(assets::primitive_mesh_kind kind) -> const char* {
  switch (kind) {
    case assets::primitive_mesh_kind::cube: return ICON_MDI_CUBE_OUTLINE;
    case assets::primitive_mesh_kind::sphere: return ICON_MDI_SPHERE;
    case assets::primitive_mesh_kind::plane: return ICON_MDI_SQUARE_OUTLINE;
    case assets::primitive_mesh_kind::capsule: return ICON_MDI_PILL;
    case assets::primitive_mesh_kind::cylinder: return ICON_MDI_CYLINDER;
  }

  return ICON_MDI_CUBE_OUTLINE;
}

[[nodiscard]] auto contains_ignore_case(std::string_view haystack, std::string_view needle) -> bool {
  const auto to_lower = [](std::string_view text) {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

auto draw_asset_picker(const char* popup_id, const asset_picker_item& current, const asset_picker_item& default_item, const asset_picker_options& options) -> asset_picker_result {
  auto result = asset_picker_result{};

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto has_current = !current.path.empty();

  ImGui::PushID(popup_id);
  ImGui::BeginGroup();

  const auto& style = ImGui::GetStyle();
  const auto field_left = ImGui::GetCursorScreenPos().x;
  const auto frame_height = ImGui::GetFrameHeight();
  const auto preview_size = std::max(options.preview_size, frame_height);

  // A preview taller than two rows stacks the name and buttons beside it, like Unity's large object fields.
  const auto is_large = preview_size >= frame_height * 2.0f + style.ItemSpacing.y;

  auto button_tile = asset_tile_desc{};
  button_tile.icon_glyph = icon_for(options.kind);
  button_tile.size = ImVec2{preview_size, preview_size};

  if (options.kind == assets::asset_type::texture && has_current) {
    button_tile.is_texture_thumbnail = true;
    button_tile.texture = assets_module.load_texture(current.path, options.load_format);
  }

  if (options.thumbnail && current.id != math::uuid::nil()) {
    button_tile.thumbnail = options.thumbnail(current.id);
  }

  const auto button_tile_result = draw_asset_tile("##button_tile", button_tile);

  if (button_tile_result.clicked) {
    ImGui::OpenPopup(popup_id);
  }

  ImGui::SameLine();

  const auto show_edit = options.show_edit_button && has_current;
  const auto show_reveal = options.show_reveal_button && has_current;

  const auto icon_button_width = [&](const char* icon) {
    return ImGui::CalcTextSize(icon).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
  };

  // In the compact layout the buttons share the name's row, so the name gives up their width to keep the field's right edge fixed.
  auto inline_buttons_width = 0.0f;

  if (!is_large) {
    inline_buttons_width += show_edit ? icon_button_width(ICON_MDI_FILE_EDIT_OUTLINE) : 0.0f;
    inline_buttons_width += show_reveal ? icon_button_width(ICON_MDI_FOLDER_SEARCH_OUTLINE) : 0.0f;
  }

  const auto name_width = options.width > 0.0f ? std::max(field_left + options.width - ImGui::GetCursorScreenPos().x - inline_buttons_width, frame_height) : 0.0f;

  if (is_large) {
    ImGui::BeginGroup();
  }

  const auto label = has_current ? current.path.filename().string() : std::string{"(None)"};

  // A Button, not a Selectable: a zero-width Selectable fills the rest of the row and swallows whatever follows.
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{0.0f, 0.5f});

  if (ImGui::Button(label.c_str(), ImVec2{name_width, 0.0f})) {
    ImGui::OpenPopup(popup_id);
  }

  ImGui::PopStyleVar();

  if (has_current && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", current.path.string().c_str());
  }

  // The large layout starts its button row on a new line under the name.
  auto is_first_button = is_large;

  const auto next_button = [&]() {
    if (!is_first_button) {
      ImGui::SameLine();
    }

    is_first_button = false;
  };

  if (show_edit) {
    next_button();

    if (ImGui::Button(ICON_MDI_FILE_EDIT_OUTLINE)) {
      result.edit_requested = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Edit this asset");
    }
  }

  if (show_reveal) {
    next_button();

    if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE)) {
      result.reveal_requested = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show in Asset Browser");
    }
  }

  if (options.extra_buttons) {
    next_button();
    options.extra_buttons();
  }

  if (is_large) {
    ImGui::EndGroup();
  }

  ImGui::EndGroup();

  // The whole closed field accepts drops, like Unity's object fields. A null type would make ImGui accept any payload.
  if (const auto* payload_type = drag_payload_type_for(options.kind); payload_type != nullptr && ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload(payload_type)) {
      auto dropped = asset_drag_payload{};
      std::memcpy(&dropped, payload->Data, sizeof(dropped));

      result.changed = true;
      result.picked = asset_picker_item{resolve_drag_id(dropped), std::filesystem::path{dropped.path}};
    }

    ImGui::EndDragDropTarget();
  }

  // Fixed width so the popup doesn't reflow while filtering.
  ImGui::SetNextWindowSize(ImVec2{340.0f, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup(popup_id)) {
    // Cleared when a different picker's popup opens. The candidates are snapshotted on open, not re-queried every frame.
    static auto filter_buffer = std::array<char, 128u>{};
    static auto last_popup_id = std::string{};
    static auto candidates = std::vector<asset_picker_item>{};

    if (ImGui::IsWindowAppearing() || last_popup_id != popup_id) {
      filter_buffer[0] = '\0';
      last_popup_id = popup_id;

      candidates.clear();

      for (auto& [id, path] : assets_module.assets_of(options.kind)) {
        candidates.push_back(asset_picker_item{id, std::move(path)});
      }

      std::ranges::sort(candidates, {}, &asset_picker_item::path);
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

    if (options.kind == assets::asset_type::mesh && options.show_builtin_primitives) {
      ImGui::TextDisabled("Built-in");

      for (const auto primitive_kind : reflection::enum_values<assets::primitive_mesh_kind>()) {
        const auto name = assets::primitive_mesh_name(primitive_kind);

        if (filter_buffer[0] != '\0' && !contains_ignore_case(name, filter_buffer.data())) {
          continue;
        }

        const auto label = std::string{icon_for(primitive_kind)} + " " + std::string{name};

        if (ImGui::MenuItem(label.c_str())) {
          result.changed = true;
          result.picked = asset_picker_item{assets::primitive_mesh_uuid(primitive_kind), std::filesystem::path{name}};
        }
      }

      ImGui::Separator();
    }

    auto visible = std::vector<const asset_picker_item*>{};

    for (const auto& candidate : candidates) {
      if (filter_buffer[0] == '\0' || contains_ignore_case(candidate.path.string(), filter_buffer.data())) {
        visible.push_back(&candidate);
      }
    }

    constexpr auto row_size = ImVec2{24.0f, 24.0f};
    const auto row_height = std::max(row_size.y, ImGui::GetTextLineHeight()) + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginChild("##asset_picker_list", ImVec2{0.0f, 260.0f})) {
      // Clipped so only on-screen rows load thumbnails.
      auto clipper = ImGuiListClipper{};
      clipper.Begin(static_cast<std::int32_t>(visible.size()), row_height);

      while (clipper.Step()) {
        for (auto index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto& candidate = *visible[static_cast<std::size_t>(index)];
          const auto relative_string = candidate.path.string();

          ImGui::PushID(index);

          auto row_tile = asset_tile_desc{};
          row_tile.icon_glyph = icon_for(options.kind);
          row_tile.size = row_size;

          if (options.kind == assets::asset_type::texture) {
            row_tile.is_texture_thumbnail = true;
            row_tile.texture = assets_module.load_texture(candidate.id, options.load_format);
          }

          if (options.thumbnail) {
            row_tile.thumbnail = options.thumbnail(candidate.id);
          }

          const auto row_tile_result = draw_asset_tile("##row_tile", row_tile);

          ImGui::SameLine();

          const auto row_clicked = ImGui::Selectable(relative_string.c_str(), false, 0, ImVec2{0.0f, row_size.y});

          if (row_clicked || row_tile_result.clicked) {
            result.changed = true;
            result.picked = candidate;
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

} // namespace sbx::render
