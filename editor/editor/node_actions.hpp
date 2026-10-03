// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_NODE_ACTIONS_HPP_
#define EDITOR_NODE_ACTIONS_HPP_

#include <vector>

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

} // namespace editor

#endif // EDITOR_NODE_ACTIONS_HPP_
