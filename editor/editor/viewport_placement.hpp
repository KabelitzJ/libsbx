// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_PLACEMENT_HPP_
#define EDITOR_VIEWPORT_PLACEMENT_HPP_

#include <optional>

#include <imgui.h>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/** @brief Where a dropped asset goes: the surface point under the cursor and, if Align to Surface is on, a rotation standing it on that surface. */
struct drop_placement {
  sbx::math::vector3 position{};
  std::optional<sbx::math::quaternion> rotation{};
}; // struct drop_placement

/**
 * @brief Where an asset dropped at a Viewport pixel lands: on the surface under the cursor, else where viewport_drop_position puts it.
 *
 * @param state The editor state (Align to Surface).
 * @param position The viewport-relative pixel.
 * @param viewport_size The viewport size in pixels.
 *
 * @return The placement.
 */
[[nodiscard]] auto viewport_drop_placement(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> drop_placement;

/**
 * @brief Godot's Snap Object to Floor: drops each selected node straight down until its bounds rest on the surface below; one undo step.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto snap_selection_to_floor(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Vertex snapping: with V held, dragging in the Viewport moves the selection so its vertex nearest the cursor (picked when the drag starts) lands on the vertex under the cursor; one undo step on release.
 *
 * @param state The editor state.
 * @param image_origin The Viewport image's top-left in screen pixels.
 * @param viewport_size The viewport size in pixels.
 * @param image_clicked Whether the image was clicked this frame (and not a toolbar or gizmo).
 *
 * @return Whether a vertex-snap drag is in progress, so the click doesn't also select.
 */
auto update_vertex_snap(editor_state& state, const ImVec2& image_origin, const sbx::math::vector2u& viewport_size, bool image_clicked) -> bool;

/**
 * @brief The Ruler tool: two clicks on surfaces measure the distance between them, drawn in the Viewport; a third click starts over.
 *
 * @param state The editor state.
 * @param image_origin The Viewport image's top-left in screen pixels.
 * @param viewport_size The viewport size in pixels.
 * @param image_clicked Whether the image was clicked this frame (and not a toolbar or gizmo).
 *
 * @return Whether the ruler is active, so clicks measure instead of select.
 */
auto update_ruler(editor_state& state, const ImVec2& image_origin, const sbx::math::vector2u& viewport_size, bool image_clicked) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_PLACEMENT_HPP_
