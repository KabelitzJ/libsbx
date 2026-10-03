// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_
#define EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief A path in the Prefabs/ folder named after @p tag, with a numeric suffix on collision, so Create Prefab needs no save dialog.
 *
 * @param tag The source node's name.
 *
 * @return The assets-relative path.
 */
auto unique_prefab_relative_path(const std::string& tag) -> std::filesystem::path;

/**
 * @brief Instantiates a dropped prefab; call inside an open BeginDragDropTarget() block.
 *
 * @param state The editor state.
 * @param scene The scene.
 * @param parent_id The parent for the instance, or nullopt for the top level.
 * @param position The instance's local position, if any.
 */
auto try_instantiate_prefab_drop(editor_state& state, sbx::scenes::scene& scene, std::optional<sbx::math::uuid> parent_id, std::optional<sbx::math::vector3> position = std::nullopt) -> void;

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PREFAB_MENU_HPP_
