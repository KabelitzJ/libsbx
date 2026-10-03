// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_WIDGETS_ASSET_PICKER_HPP_
#define LIBSBX_RENDER_UI_WIDGETS_ASSET_PICKER_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/types.hpp>

namespace sbx::render {

enum class asset_picker_kind : std::uint8_t {
  texture,
  mesh,
  material,
  particle_effect,
  animation_graph,
  shader_graph,
  font,
}; // enum class asset_picker_kind

/**
 * @brief The drag payload type a tile must carry to be dropped onto a picker of @p kind.
 *
 * @param kind The picker kind.
 *
 * @return The payload type string.
 */
[[nodiscard]] auto drag_payload_type_for(asset_picker_kind kind) -> const char*;

struct asset_picker_item {
  sbx::math::uuid id{sbx::math::uuid::nil()};
  std::filesystem::path path{}; // project-relative
}; // struct asset_picker_item

struct asset_picker_options {
  asset_picker_kind kind{asset_picker_kind::texture};
  std::vector<std::string> extensions{}; // e.g. {".png", ".jpg", ".jpeg"}; matched against path::extension()
  bool allow_none{false};         // offers a "(None)" entry that clears the slot
  bool show_edit_button{false};   // a second button next to the picker that sets edit_requested
  bool show_reveal_button{false}; // a button that sets reveal_requested ("show in Asset Browser")
  bool show_builtin_primitives{false}; // mesh kind only: lists the built-in primitives above the files
  sbx::graphics::format load_format{sbx::graphics::format::r8g8b8a8_srgb}; // texture kind only
}; // struct asset_picker_options

struct asset_picker_result {
  bool changed{false};          // picked, cleared or reset: the caller should reassign its slot
  bool cleared{false};          // "(None)" was picked; changed is also set and picked is empty
  bool edit_requested{false};   // edit button clicked
  bool reveal_requested{false}; // reveal button clicked
  bool reset_to_default{false}; // "Reset to Default" clicked; picked is default_item
  asset_picker_item picked{};
}; // struct asset_picker_result

/**
 * @brief A button showing the current asset that opens a filterable thumbnail list, and accepts dropped tiles of the matching kind.
 *
 * Only resolves which asset was picked; callers load it, since the load_* functions take different options.
 *
 * @param popup_id Unique ImGui id for the picker's popup and widgets.
 * @param current The slot's current asset, or a default item if empty.
 * @param default_item A non-empty path enables "Reset to Default", reseeding the slot from this item.
 * @param options Kind, filters and optional buttons.
 *
 * @return What the user did this frame.
 */
[[nodiscard]] auto draw_asset_picker(const char* popup_id, const asset_picker_item& current, const asset_picker_item& default_item, const asset_picker_options& options) -> asset_picker_result;

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_WIDGETS_ASSET_PICKER_HPP_
