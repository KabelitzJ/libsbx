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

namespace sbx::render::widgets {

enum class asset_picker_kind : std::uint8_t {
  texture,
  mesh,
  material,
  particle_effect,
  animation_graph,
}; // enum class asset_picker_kind

/** @brief The drag_drop_payload_* string (asset_tile.hpp) a dragged tile must carry to be droppable onto a picker of this kind. */
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
  sbx::graphics::format load_format{sbx::graphics::format::r8g8b8a8_srgb}; // texture kind only
}; // struct asset_picker_options

struct asset_picker_result {
  bool changed{false};          // picked, cleared, or reset_to_default -- caller should reassign its slot
  bool cleared{false};          // "(None)" was picked -- changed is also true; picked is empty
  bool edit_requested{false};   // edit button clicked; caller performs its own "jump to this asset"
  bool reveal_requested{false}; // reveal button clicked; caller performs its own "show in Asset Browser"
  bool reset_to_default{false}; // "Reset to Default" clicked; picked is default_item
  asset_picker_item picked{};
}; // struct asset_picker_result

/**
 * @brief Button-plus-popup asset picker: shows the current selection's thumbnail/icon and name,
 * opens a filterable, thumbnail-rendered list on click, and doubles as a drag-and-drop target (so
 * dropping a matching asset from the Asset Browser directly onto the closed button works too).
 *
 * Does not itself call assets_module::load_*() -- only resolves which asset was picked. Callers
 * load the handle themselves, since load_mesh/load_material/etc. differ in what extra options
 * (mesh_import_options, format, ...) they take.
 *
 * @param popup_id Unique ImGui ID/label for this picker's popup and internal widget IDs.
 * @param current The slot's current asset, or a default-constructed item if empty.
 * @param default_item Non-empty path enables a "Reset to Default" entry (e.g. a mesh's own submesh
 * material) that reseeds the slot from this item instead of a file under the assets directory.
 */
[[nodiscard]] auto draw_asset_picker(const char* popup_id, const asset_picker_item& current, const asset_picker_item& default_item, const asset_picker_options& options) -> asset_picker_result;

} // namespace sbx::render::widgets

#endif // LIBSBX_RENDER_UI_WIDGETS_ASSET_PICKER_HPP_
