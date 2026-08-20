// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_PICKING_HPP_
#define EDITOR_VIEWPORT_PICKING_HPP_

#include <libsbx/math/vector2.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Picks the scene node under a viewport-relative pixel position (0,0 at the viewport
 * image's top-left) and selects it in @p state, mirroring how the Hierarchy panel selects a node.
 * Casts a ray from the active camera through the clicked pixel and tests it against every mesh
 * renderer's world-space bounds, keeping the nearest hit. A miss (nothing hit, or no active
 * camera) clears the selection, same as clicking empty space in the Hierarchy.
 */
auto pick_node_at_viewport_position(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void;

} // namespace editor

#endif // EDITOR_VIEWPORT_PICKING_HPP_
