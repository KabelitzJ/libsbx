// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_LAYER_FIELDS_HPP_
#define EDITOR_WIDGETS_LAYER_FIELDS_HPP_

#include <cstdint>

#include <libsbx/scenes/components.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief A node's layer (0-31) as a dropdown of the project's named layers plus "Edit Layers...".
 *
 * @param state The editor state.
 * @param label The widget label.
 * @param layer_index The layer to edit.
 *
 * @return True if @p layer_index changed.
 */
auto draw_layer_combo(editor_state& state, const char* label, std::uint8_t& layer_index) -> bool;

/**
 * @brief A layer mask as a button summarizing it that opens a checklist of named layers with Nothing/Everything and "Edit Layers...".
 *
 * @param state The editor state.
 * @param label The widget label.
 * @param mask The mask to edit.
 *
 * @return True if @p mask changed.
 */
auto draw_layer_mask_field(editor_state& state, const char* label, sbx::scenes::layer_mask& mask) -> bool;

} // namespace editor

#endif // EDITOR_WIDGETS_LAYER_FIELDS_HPP_
