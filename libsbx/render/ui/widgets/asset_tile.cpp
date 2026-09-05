// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/asset_tile.hpp>

#include <algorithm>
#include <cfloat>
#include <cstring>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/resources/sampler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/ui_module.hpp>

namespace sbx::render::widgets {

namespace {

// Lazily constructed on first use, same idiom as editor_ui_layer's own viewport sampler — shared
// by every tile's thumbnail for the process's lifetime.
[[nodiscard]] auto thumbnail_sampler() -> VkSampler {
  static auto sampler = graphics::sampler{graphics::sampler::create_info{
    .mag_filter = graphics::filter::linear,
    .min_filter = graphics::filter::linear,
    .mipmap_mode = graphics::mipmap_mode::linear,
    .address_mode_u = graphics::address_mode::clamp_to_edge,
    .address_mode_v = graphics::address_mode::clamp_to_edge,
    .address_mode_w = graphics::address_mode::clamp_to_edge,
    .max_anisotropy = 1.0f,
    .max_lod = graphics::lod_clamp::none,
    .name = "Asset Tile Thumbnail Sampler",
  }};

  return sampler.handle();
}

} // namespace

auto draw_asset_tile(const char* id, const asset_tile_desc& desc) -> asset_tile_result {
  auto result = asset_tile_result{};

  ImGui::PushID(id);

  auto* draw_list = ImGui::GetWindowDrawList();
  const auto tile_min = ImGui::GetCursorScreenPos();
  const auto tile_max = ImVec2{tile_min.x + desc.size.x, tile_min.y + desc.size.y};

  ImGui::InvisibleButton("##tile", desc.size);

  result.hovered = ImGui::IsItemHovered();

  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
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

  if (desc.is_texture_thumbnail && desc.texture.is_valid()) {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    if (assets_module.is_resident(desc.texture)) {
      const auto view = assets_module.image_view_of(desc.texture);

      if (view != VK_NULL_HANDLE) {
        auto& ui_module = core::engine::get_module<render::ui_module>();
        const auto texture_id = ui_module.texture_id(view, thumbnail_sampler());

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

  if (desc.drag_payload_type != nullptr && !desc.is_directory && ImGui::BeginDragDropSource()) {
    auto payload = asset_drag_payload{desc.drag_id, {}};

    const auto path_string = desc.drag_path.string();
    const auto copy_length = std::min(path_string.size(), sizeof(payload.path) - 1u);
    std::memcpy(payload.path, path_string.data(), copy_length);
    payload.path[copy_length] = '\0';

    ImGui::SetDragDropPayload(desc.drag_payload_type, &payload, sizeof(payload));

    ImGui::TextUnformatted(!desc.display_name.empty() ? desc.display_name.c_str() : desc.drag_path.filename().string().c_str());

    ImGui::EndDragDropSource();
  }

  ImGui::PopID();

  return result;
}

} // namespace sbx::render::widgets
