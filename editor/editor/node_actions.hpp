// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_NODE_ACTIONS_HPP_
#define EDITOR_NODE_ACTIONS_HPP_

#include <span>
#include <vector>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief The ids without an ancestor in the list: the roots subtree operations act on, in the original order.
 *
 * @param scene The scene.
 * @param ids The selected ids.
 *
 * @return The root ids.
 */
[[nodiscard]] auto selection_roots(sbx::scenes::scene& scene, const std::vector<sbx::math::uuid>& ids) -> std::vector<sbx::math::uuid>;

/**
 * @brief Ctrl+C: snapshots every selected subtree into the clipboard.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto copy_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Ctrl+V: inserts the clipboard with fresh ids after the primary selection (or at the end of the top level) and selects it, as one undo step.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto paste_clipboard(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Ctrl+D: duplicates every selected subtree right after its original and selects the copies, as one undo step.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto duplicate_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Delete: deletes every selected subtree as one undo step.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto delete_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief F: backs the editor camera along its view direction until the selection's bounds fill the view.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto focus_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief The world-space bounds of the selection: its meshes and node origins, children included.
 *
 * @param state The editor state.
 * @param scene The scene.
 *
 * @return The bounds; empty without a selection.
 */
[[nodiscard]] auto selection_bounds(editor_state& state, sbx::scenes::scene& scene) -> sbx::math::volume;

/**
 * @brief The world-space bounds of some nodes and their descendants: meshes and node origins.
 *
 * @param scene The scene.
 * @param roots The nodes.
 *
 * @return The bounds; empty for no valid node.
 */
[[nodiscard]] auto subtree_bounds(sbx::scenes::scene& scene, std::span<const sbx::math::uuid> roots) -> sbx::math::volume;

/**
 * @brief Focus Selection: frames the selection, and a second press within half a second keeps the camera following it, like Godot.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto focus_or_follow_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Moves the camera with the followed selection each frame; stops once the camera is moved by hand or the selection is gone.
 *
 * @param state The editor state.
 * @param scene The scene.
 */
auto update_selection_follow(editor_state& state, sbx::scenes::scene& scene) -> void;

/**
 * @brief Gives the selected nodes the editor camera's rotation, and its position too if @p include_position; one undo step.
 *
 * @param state The editor state.
 * @param scene The scene.
 * @param include_position Align Transform with View (true) or Align Rotation with View (false).
 */
auto align_selection_with_view(editor_state& state, sbx::scenes::scene& scene, bool include_position) -> void;

} // namespace editor

#endif // EDITOR_NODE_ACTIONS_HPP_
