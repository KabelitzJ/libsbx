// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_WIDGETS_ASSET_TILE_HPP_
#define LIBSBX_RENDER_UI_WIDGETS_ASSET_TILE_HPP_

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

#include <imgui.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture2d.hpp>

namespace sbx::render {

// One payload type per asset kind: ImGui only matches drops of the same type, so wrong-kind drops are no-ops. ImGui caps these at 32 characters.
inline constexpr auto drag_drop_payload_texture = "SBX_ASSET_TEXTURE";
inline constexpr auto drag_drop_payload_mesh = "SBX_ASSET_MESH";
inline constexpr auto drag_drop_payload_material = "SBX_ASSET_MATERIAL";
inline constexpr auto drag_drop_payload_particle_effect = "SBX_ASSET_PARTICLE_EFFECT";
inline constexpr auto drag_drop_payload_animation_graph = "SBX_ASSET_ANIM_GRAPH";
inline constexpr auto drag_drop_payload_shader_graph = "SBX_ASSET_SHADER_GRAPH";
inline constexpr auto drag_drop_payload_font = "SBX_ASSET_FONT";
inline constexpr auto drag_drop_payload_prefab = "SBX_ASSET_PREFAB";

/** @brief A tile's drag payload; trivially copyable because ImGui memcpys payloads. */
struct asset_drag_payload {
  sbx::math::uuid id{sbx::math::uuid::nil()};
  char path[256]{}; // project-relative, null-terminated
}; // struct asset_drag_payload

/**
 * @brief One asset tile: a texture preview or a tinted icon glyph, plus selection and drag source wiring.
 *
 * Kind-agnostic, so browser-only kinds need no shared enum. Draws only the square; callers place labels, and display_name is only the drag tooltip.
 */
struct asset_tile_desc {
  // Icon mode: a Material Design Icons glyph, tinted.
  const char* icon_glyph{};
  ImU32 icon_tint{IM_COL32_WHITE};

  // Texture mode: previews `texture`, falling back to the icon while it isn't resident.
  bool is_texture_thumbnail{false};
  sbx::assets::texture2d_handle texture{};

  bool is_directory{false};
  std::string display_name{}; // drag-preview text only

  // Drag source, offered only with a drag_payload_type and not for directories.
  const char* drag_payload_type{};
  sbx::math::uuid drag_id{sbx::math::uuid::nil()};
  std::filesystem::path drag_path{};

  const char* secondary_drag_payload_type{};
  const void* secondary_drag_payload_data{};
  std::size_t secondary_drag_payload_size{0u};

  bool is_selected{false};
  ImVec2 size{64.0f, 64.0f};
}; // struct asset_tile_desc

struct asset_tile_result {
  bool clicked{false};
  bool double_clicked{false};
  bool hovered{false};
}; // struct asset_tile_result

/**
 * @brief Draws one tile with background, selection highlight and optional drag source; shared by the Asset Browser grid and the Inspector's pickers.
 *
 * @param id Scopes the tile's ImGui ids; must be unique among sibling tiles.
 * @param desc What to draw.
 *
 * @return The tile's interaction result.
 */
[[nodiscard]] auto draw_asset_tile(const char* id, const asset_tile_desc& desc) -> asset_tile_result;

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_WIDGETS_ASSET_TILE_HPP_
