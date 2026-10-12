// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/asset_tile.hpp>

#include <algorithm>
#include <cfloat>
#include <cstring>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/ui_module.hpp>
#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace sbx::render {

auto drag_payload_type_for(assets::asset_type type) -> const char* {
  switch (type) {
    case assets::asset_type::texture: return drag_drop_payload_texture;
    case assets::asset_type::mesh: return drag_drop_payload_mesh;
    case assets::asset_type::material: return drag_drop_payload_material;
    case assets::asset_type::particle_effect: return drag_drop_payload_particle_effect;
    case assets::asset_type::animation_graph: return drag_drop_payload_animation_graph;
    case assets::asset_type::shader_graph: return drag_drop_payload_shader_graph;
    case assets::asset_type::font: return drag_drop_payload_font;
    case assets::asset_type::audio_clip: return drag_drop_payload_audio_clip;
    case assets::asset_type::scene: return drag_drop_payload_scene;
    case assets::asset_type::prefab: return drag_drop_payload_prefab;
    default: return nullptr;
  }
}

auto icon_for(assets::asset_type type) -> const char* {
  switch (type) {
    case assets::asset_type::texture: return ICON_MDI_IMAGE;
    case assets::asset_type::mesh: return ICON_MDI_CUBE_OUTLINE;
    case assets::asset_type::material: return ICON_MDI_PALETTE_SWATCH;
    case assets::asset_type::environment_map: return ICON_MDI_EARTH;
    case assets::asset_type::particle_effect: return ICON_MDI_FIREWORK;
    case assets::asset_type::animation_graph: return ICON_MDI_STATE_MACHINE;
    case assets::asset_type::shader_graph: return ICON_MDI_VECTOR_POLYLINE;
    case assets::asset_type::font: return ICON_MDI_FORMAT_FONT;
    case assets::asset_type::audio_clip: return ICON_MDI_MUSIC_NOTE;
    case assets::asset_type::prefab: return ICON_MDI_CUBE_SCAN;
    case assets::asset_type::scene: return ICON_MDI_FILE_TREE;
    case assets::asset_type::script: return ICON_MDI_FILE_CODE_OUTLINE;
    case assets::asset_type::unknown: return ICON_MDI_FILE_OUTLINE;
  }

  return ICON_MDI_FILE_OUTLINE;
}

auto resolve_drag_id(const asset_drag_payload& payload) -> math::uuid {
  if (payload.id != math::uuid::nil() || payload.path[0] == '\0') {
    return payload.id;
  }

  const auto& project = core::engine::project();

  return core::engine::get_module<assets::assets_module>().find(project.assets_directory() / payload.path);
}

auto draw_asset_tile(const char* id, const asset_tile_desc& desc) -> asset_tile_result {
  auto result = asset_tile_result{};

  ImGui::PushID(id);

  auto* draw_list = ImGui::GetWindowDrawList();
  const auto tile_min = ImGui::GetCursorScreenPos();
  const auto tile_max = ImVec2{tile_min.x + desc.size.x, tile_min.y + desc.size.y};

  // InvisibleButton's release-based result, not IsItemClicked: a press-based click changed the selection before a drag from this tile could land.
  const auto pressed = ImGui::InvisibleButton("##tile", desc.size);

  result.hovered = ImGui::IsItemHovered();

  if (pressed) {
    result.clicked = true;
    result.double_clicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  }

  const auto& style = ImGui::GetStyle();
  const auto background = result.hovered ? ImGui::GetColorU32(ImGuiCol_HeaderHovered) : ImGui::GetColorU32(ImGuiCol_FrameBg);

  draw_list->AddRectFilled(tile_min, tile_max, background, style.FrameRounding);

  if (desc.is_selected) {
    draw_list->AddRect(tile_min, tile_max, ImGui::GetColorU32(ImGuiCol_HeaderActive), style.FrameRounding, 0, 2.0f);
  }

  auto drew_thumbnail = false;

  if (desc.thumbnail.has_value()) {
    const auto padding = desc.size.x * 0.06f;
    draw_list->AddImage(*desc.thumbnail, ImVec2{tile_min.x + padding, tile_min.y + padding}, ImVec2{tile_max.x - padding, tile_max.y - padding});

    drew_thumbnail = true;
  } else if (desc.is_texture_thumbnail && desc.texture.is_valid()) {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    if (assets_module.is_resident(desc.texture)) {
      const auto view = assets_module.image_view_of(desc.texture);

      if (view != VK_NULL_HANDLE) {
        auto& ui_module = core::engine::get_module<render::ui_module>();
        const auto texture_id = ui_module.texture_id(view, ui_module.thumbnail_sampler());

        const auto padding = desc.size.x * 0.06f;
        draw_list->AddImage(texture_id, ImVec2{tile_min.x + padding, tile_min.y + padding}, ImVec2{tile_max.x - padding, tile_max.y - padding});

        drew_thumbnail = true;
      }
    }
  }

  if (!drew_thumbnail && desc.icon_glyph != nullptr) {
    auto* font = ImGui::GetFont();
    const auto font_size = desc.size.y * 0.55f;
    const auto text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, desc.icon_glyph);
    const auto icon_pos = ImVec2{
      tile_min.x + (desc.size.x - text_size.x) * 0.5f,
      tile_min.y + (desc.size.y - text_size.y) * 0.5f,
    };

    draw_list->AddText(font, font_size, icon_pos, desc.icon_tint, desc.icon_glyph);
  }

  const auto offers_kind_drag = desc.drag_payload_type != nullptr && !desc.is_directory;
  const auto offers_secondary_drag = desc.secondary_drag_payload_type != nullptr && desc.secondary_drag_payload_data != nullptr;

  if ((offers_kind_drag || offers_secondary_drag) && ImGui::BeginDragDropSource()) {
    if (offers_kind_drag) {
      auto payload = asset_drag_payload{desc.drag_id, {}};

      const auto path_string = desc.drag_path.string();
      const auto copy_length = std::min(path_string.size(), sizeof(payload.path) - 1u);
      std::memcpy(payload.path, path_string.data(), copy_length);
      payload.path[copy_length] = '\0';

      ImGui::SetDragDropPayload(desc.drag_payload_type, &payload, sizeof(payload));
    }

    if (offers_secondary_drag) {
      ImGui::SetDragDropPayload(desc.secondary_drag_payload_type, desc.secondary_drag_payload_data, desc.secondary_drag_payload_size);
    }

    ImGui::TextUnformatted(!desc.display_name.empty() ? desc.display_name.c_str() : desc.drag_path.filename().string().c_str());

    ImGui::EndDragDropSource();
  }

  ImGui::PopID();

  return result;
}

} // namespace sbx::render
