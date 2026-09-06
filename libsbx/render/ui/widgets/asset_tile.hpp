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
#include <libsbx/assets/texture.hpp>

namespace sbx::render::widgets {

// One drag-and-drop payload type string per asset kind a picker exists for — ImGui only matches a
// drop target against a payload registered under the exact same type string, so this is also what
// makes a texture dropped on a mesh slot a no-op with zero validation code (see asset_picker.hpp).
// Kept short: ImGui caps payload type strings at 32 characters.
inline constexpr auto drag_drop_payload_texture = "SBX_ASSET_TEXTURE";
inline constexpr auto drag_drop_payload_mesh = "SBX_ASSET_MESH";
inline constexpr auto drag_drop_payload_material = "SBX_ASSET_MATERIAL";
inline constexpr auto drag_drop_payload_particle_effect = "SBX_ASSET_PARTICLE_EFFECT";
inline constexpr auto drag_drop_payload_animation_graph = "SBX_ASSET_ANIM_GRAPH";

/** @brief What a tile's drag source carries; ImGui payloads are memcpy'd, so this stays trivially copyable. */
struct asset_drag_payload {
  sbx::math::uuid id{sbx::math::uuid::nil()};
  char path[256]{}; // project-relative, null-terminated
}; // struct asset_drag_payload

/**
 * @brief What to draw for one asset_tile: either a real GPU texture preview or a tinted icon
 * glyph, plus the selection/drag-source wiring shared by every caller.
 *
 * Deliberately has no notion of "asset kind" — callers (the asset browser, the asset picker)
 * already know their own kind and just pick a glyph/tint or a texture to preview. This keeps the
 * widget usable for browser-only kinds (scene, script, environment_map) that no picker ever
 * needs, without extending some shared enum every time a new browsable kind is added.
 *
 * Draws only the square itself — no on-tile label. Callers place their own label text below (grid
 * layout) or beside (popup list row) the tile, however their layout needs it; display_name below
 * is used only as the drag-preview tooltip.
 */
struct asset_tile_desc {
  // Icon path (is_texture_thumbnail == false): a Material Design Icons glyph, tinted.
  const char* icon_glyph{};
  ImU32 icon_tint{IM_COL32_WHITE};

  // Texture-thumbnail path (is_texture_thumbnail == true): tries a real GPU preview of `texture`,
  // falling back to icon_glyph/icon_tint above if it isn't resident yet (still uploading) or is
  // an empty handle.
  bool is_texture_thumbnail{false};
  sbx::assets::texture_handle texture{};

  bool is_directory{false};
  std::string display_name{}; // drag-preview text only; see doc comment above

  // Drag source, only offered when drag_payload_type is non-null and !is_directory.
  const char* drag_payload_type{};
  sbx::math::uuid drag_id{sbx::math::uuid::nil()};
  std::filesystem::path drag_path{};

  bool is_selected{false};
  ImVec2 size{64.0f, 64.0f};
}; // struct asset_tile_desc

struct asset_tile_result {
  bool clicked{false};
  bool double_clicked{false};
  bool hovered{false};
}; // struct asset_tile_result

/**
 * @brief Draws one square tile: thumbnail-or-icon, background, selection highlight, and (when
 * requested) a drag-and-drop source. The shared primitive behind both the Asset Browser's grid
 * and the Inspector's asset pickers — see asset_picker.hpp.
 * @param id Scopes the tile's ImGui IDs; must be unique among sibling tiles (e.g. the entry's path).
 */
[[nodiscard]] auto draw_asset_tile(const char* id, const asset_tile_desc& desc) -> asset_tile_result;

} // namespace sbx::render::widgets

#endif // LIBSBX_RENDER_UI_WIDGETS_ASSET_TILE_HPP_
