// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/widgets/asset_tile.hpp>

namespace editor {

auto asset_browser_panel::_draw_asset_grid(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  // Search matches, in _cached_entries' order.
  auto visible = std::vector<std::size_t>{};

  for (auto index = std::size_t{0u}; index < _cached_entries.size(); ++index) {
    if (filter_matches(_cached_entries[index].path.filename().string(), _search_filter.data())) {
      visible.push_back(index);
    }
  }

  if (visible.empty()) {
    ImGui::TextDisabled("Nothing here.");
  }

  const auto avail_width = ImGui::GetContentRegionAvail().x;
  const auto cell_width = _tile_size + 8.0f;
  const auto columns = std::max(std::int32_t{1}, static_cast<std::int32_t>(avail_width / cell_width));

  const auto row_height = _tile_size + ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;
  const auto row_count = (static_cast<std::int32_t>(visible.size()) + columns - 1) / columns;

  // Clipped by row so only on-screen tiles load thumbnails.
  auto clipper = ImGuiListClipper{};
  clipper.Begin(row_count, row_height);

  while (clipper.Step()) {
    for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      for (auto column = std::int32_t{0}; column < columns; ++column) {
        const auto visible_index = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column);

        if (visible_index >= visible.size()) {
          break;
        }

        if (column > 0) {
          ImGui::SameLine();
        }

        auto& entry = _cached_entries[visible[visible_index]];
        const auto is_renaming_this = entry.path == _renaming_path;

        ImGui::PushID(entry.path.string().c_str());
        ImGui::BeginGroup();

        auto tile_desc = sbx::render::asset_tile_desc{};
        tile_desc.icon_glyph = icon_for(entry);
        tile_desc.is_directory = entry.is_directory;
        tile_desc.is_selected = is_entry_selected(state, entry.path);
        tile_desc.size = ImVec2{_tile_size, _tile_size};
        tile_desc.display_name = entry.path.filename().string();
        tile_desc.drag_payload_type = sbx::render::drag_payload_type_for(entry.kind);
        tile_desc.drag_id = entry.id;
        tile_desc.drag_path = entry.path;

        const auto move_payload = make_move_drag_payload(entry.path);
        tile_desc.secondary_drag_payload_type = asset_move_drag_payload_type;
        tile_desc.secondary_drag_payload_data = &move_payload;
        tile_desc.secondary_drag_payload_size = sizeof(move_payload);

        // By uuid only: loading by path would import a new file with default settings before the import window shows.
        if (entry.kind == sbx::assets::asset_type::texture && entry.id != sbx::math::uuid::nil()) {
          tile_desc.is_texture_thumbnail = true;
          tile_desc.texture = assets_module.load_texture(entry.id);
        } else if (state.thumbnails != nullptr) {
          tile_desc.thumbnail = state.thumbnails->get(entry.id, entry.kind);
        }

        const auto tile_result = sbx::render::draw_asset_tile("##tile", tile_desc);

        if (tile_result.hovered) {
          ImGui::SetTooltip("%s", entry.path.string().c_str());
        }

        if (!is_renaming_this && tile_result.clicked) {
          if (entry.is_directory) {
            _navigate_to(entry.path);
          } else if (sbx::assets::is_manifested(entry.kind)) {
            if (!_defer_import_if_unseen(state, entry.path)) {
              // entry.path is relative to assets_directory().
              entry.id = assets_module.import(project.assets_directory() / entry.path);
              state.select_asset(entry.id, entry.path, entry.kind);
            }
          } else if (entry.kind == sbx::assets::asset_type::script) {
            state.select_asset(sbx::math::uuid::nil(), entry.path, sbx::assets::asset_type::script);
          }

          // The click branch above already resolved entry.id.
          if (tile_result.double_clicked && entry.kind == sbx::assets::asset_type::animation_graph) {
            state.request_open_animation_graph_editor(entry.id, entry.path);
          }

          if (tile_result.double_clicked && entry.kind == sbx::assets::asset_type::shader_graph) {
            state.request_open_shader_graph_editor(entry.id, entry.path);
          }

          if (tile_result.double_clicked && entry.kind == sbx::assets::asset_type::scene) {
            state.request_open_scene(entry.path);
          }
        }

        if (entry.is_directory) {
          _draw_move_drop_target(state, entry.path);
        }

        if (is_renaming_this) {
          _draw_rename_field(state, _tile_size);
        } else {
          const auto label = truncate_to_width(entry.path.filename().string(), _tile_size);
          const auto label_width = ImGui::CalcTextSize(label.c_str()).x;
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (_tile_size - label_width) * 0.5f));
          ImGui::TextUnformatted(label.c_str());
        }

        ImGui::EndGroup();

        if (ImGui::BeginPopupContextItem("##tile_context")) {
          _draw_entry_context_menu(state, entry);
          ImGui::EndPopup();
        }

        ImGui::PopID();
      }
    }
  }

  // Right-clicking the contents pane's empty area.
  if (ImGui::BeginPopupContextWindow("##asset_browser_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
    _draw_create_menu(state, _current_directory);
    ImGui::EndPopup();
  }
}

} // namespace editor
