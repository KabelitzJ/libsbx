// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_NODE_ACTIONS_HPP_
#define EDITOR_NODE_ACTIONS_HPP_

#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/** @brief ids minus every id that has another id from the list as an ancestor (any depth) -- the nodes a subtree-level operation (delete, move, copy) should act on. Keeps ids' order. */
[[nodiscard]] auto selection_roots(sbx::scenes::scene& scene, const std::vector<sbx::math::uuid>& ids) -> std::vector<sbx::math::uuid>;

/** @brief Ctrl+C: snapshots every selected subtree into state.node_clipboard. */
auto copy_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/** @brief Ctrl+V: inserts the clipboard (fresh ids) right after the primary selected node, or at the end of the top level if nothing is selected, and selects the result. One undo step. */
auto paste_clipboard(editor_state& state, sbx::scenes::scene& scene) -> void;

/** @brief Ctrl+D: copies every selected subtree (fresh ids) in place, right after its original, and selects the copies. One undo step. */
auto duplicate_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/** @brief Delete: deletes every selected subtree. One undo step. */
auto delete_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

/** @brief F: moves the editor camera back along its current view direction until the selection's bounds (meshes of the whole subtree, else its positions) fill the view. */
auto focus_selection(editor_state& state, sbx::scenes::scene& scene) -> void;

} // namespace editor

#endif // EDITOR_NODE_ACTIONS_HPP_
