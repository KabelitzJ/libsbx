// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_
#define EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief A dedicated Prefabs/ folder next to the rest of the assets directory, auto-named from the
 * source node's own tag with a numeric suffix on collision -- "Create Prefab..." has no target
 * directory of its own to work from (unlike the Asset Browser's own Create menu), so this picks
 * one instead of prompting for a save location every time.
 */
auto unique_prefab_relative_path(const std::string& tag) -> std::filesystem::path;

/**
 * @brief Shared by every prefab drop target in the Hierarchy -- must be called from inside an
 * already-open ImGui::BeginDragDropTarget()/EndDragDropTarget() block, same convention as the plain
 * AcceptDragDropPayload(node_drag_drop_payload_type, ...) calls right next to each call site.
 */
auto try_instantiate_prefab_drop(editor_state& state, sbx::scenes::scene& scene, std::optional<sbx::math::uuid> parent_id) -> void;

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_
