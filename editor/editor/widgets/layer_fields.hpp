// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_LAYER_FIELDS_HPP_
#define EDITOR_WIDGETS_LAYER_FIELDS_HPP_

#include <cstdint>

#include <libsbx/scenes/components.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief A node's single named layer (0-31), drawn as a dropdown over core::engine::project()'s
 * currently-named layers plus a trailing "Edit Layers..." row (fires
 * editor_state::request_open_edit_layers_popup). Returns true if @p layer_index changed.
 */
auto draw_layer_combo(editor_state& state, const char* label, std::uint8_t& layer_index) -> bool;

/**
 * @brief A 32-bit sbx::scenes::layer_mask, drawn as a button (showing "Everything"/"Nothing"/the
 * named layers it includes) that opens a checklist popup -- one checkbox per named layer, plus
 * "Nothing"/"Everything" quick-set rows and a trailing "Edit Layers..." row. Returns true if
 * @p mask changed.
 */
auto draw_layer_mask_field(editor_state& state, const char* label, sbx::scenes::layer_mask& mask) -> bool;

} // namespace editor

#endif // EDITOR_WIDGETS_LAYER_FIELDS_HPP_
