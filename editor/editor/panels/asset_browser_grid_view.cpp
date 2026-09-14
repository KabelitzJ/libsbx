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

// The tile grid pane: search-filtered, clipped by row, drawn into whatever child window the
// caller already opened.
auto asset_browser_panel::_draw_asset_grid(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  // Entries matching the search box, keeping _cached_entries' existing order (directories
  // first, then case-insensitive alphabetical).
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

  // Clipped by grid row so a folder full of textures only ever loads/thumbnails the tiles
  // actually on screen, instead of every texture in it every frame the panel is open.
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

        // Every other kind resolves entry.id lazily, on click (see asset_browser_entry's doc
        // comment) — deliberately, since an importable kind may need a dialog first (mesh's
        // "extract materials?" choice). prefab has no such dialog, so resolve it here instead:
        // a drag started on a tile that's never been clicked ends on release, over a *different*
        // widget (the drop target) — draw_asset_tile's own click detection (InvisibleButton's
        // release-based "pressed") never fires on this tile during that gesture, so a
        // click-resolved id would still be nil for the entire drag, and the payload it carries
        // would be too.
        if (entry.kind == asset_kind::prefab && entry.id == sbx::math::uuid::nil()) {
          entry.id = assets_module.import(project.assets_directory() / entry.path);
        }

        auto tile_desc = sbx::render::asset_tile_desc{};
        tile_desc.icon_glyph = icon_for(entry);
        tile_desc.is_directory = entry.is_directory;
        tile_desc.is_selected = is_entry_selected(state, entry.path);
        tile_desc.size = ImVec2{_tile_size, _tile_size};
        tile_desc.display_name = entry.path.filename().string();
        tile_desc.drag_payload_type = drag_payload_type_for(entry.kind);
        tile_desc.drag_id = entry.id;
        tile_desc.drag_path = entry.path;

        const auto move_payload = make_move_drag_payload(entry.path);
        tile_desc.secondary_drag_payload_type = asset_move_drag_payload_type;
        tile_desc.secondary_drag_payload_data = &move_payload;
        tile_desc.secondary_drag_payload_size = sizeof(move_payload);

        if (entry.kind == asset_kind::texture) {
          tile_desc.is_texture_thumbnail = true;
          tile_desc.texture = assets_module.load_texture(entry.path);
        }

        const auto tile_result = sbx::render::draw_asset_tile("##tile", tile_desc);

        if (tile_result.hovered) {
          ImGui::SetTooltip("%s", entry.path.string().c_str());
        }

        if (!is_renaming_this && tile_result.clicked) {
          if (entry.is_directory) {
            _navigate_to(entry.path);
          } else if (entry.is_importable) {
            if (entry.kind != asset_kind::mesh || !_defer_mesh_import_if_unseen(entry.path)) {
              // Same resolution requirement as above — entry.path is relative to assets_directory().
              entry.id = assets_module.import(project.assets_directory() / entry.path);
              state.select_asset(entry.id, entry.path, entry.kind);
            }
          } else if (entry.kind == asset_kind::prefab) {
            // Not importable (no cook step), but still a real manifest-registered, uuid-bearing
            // asset -- unlike scene/script below, it needs a resolved id both for the Inspector
            // and for a drag started from this tile (asset_tile_desc::drag_id, set from entry.id
            // right below where this tile is built) to carry a working uuid instead of nil.
            entry.id = assets_module.import(project.assets_directory() / entry.path);
            state.select_asset(entry.id, entry.path, entry.kind);
          } else if (entry.kind == asset_kind::scene) {
            state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::scene);
          } else if (entry.kind == asset_kind::script) {
            state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::script);
          }

          // A regular click already ran above (double_clicked implies clicked -- see
          // asset_tile.cpp), so entry.id is already resolved by the importable-kind branch.
          if (tile_result.double_clicked && entry.kind == asset_kind::animation_graph) {
            state.request_open_animation_graph_editor(entry.id, entry.path);
          }

          if (tile_result.double_clicked && entry.kind == asset_kind::shader_graph) {
            state.request_open_shader_graph_editor(entry.id, entry.path);
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

  // Right-click the empty area of the contents pane (not an entry — see NoOpenOverItems).
  if (ImGui::BeginPopupContextWindow("##asset_browser_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
    _draw_create_menu(state, _current_directory);
    ImGui::EndPopup();
  }
}

} // namespace editor
