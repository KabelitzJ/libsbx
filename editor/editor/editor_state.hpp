// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_STATE_HPP_
#define EDITOR_EDITOR_STATE_HPP_

#include <filesystem>
#include <utility>
#include <variant>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

namespace editor {

/**
 * @brief What kind of asset a file in the project's assets directory is, inferred from extension.
 */
enum class asset_kind {
  unknown,
  texture,
  mesh,
  material,
  environment_map,
  scene, // .yaml, reference-only: not routed through assets_module::import
}; // enum class asset_kind

/** @brief Nothing is selected. */
struct empty_selection { };

/** @brief A scene node is selected, identified by its uuid. */
struct node_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()};
}; // struct node_selection

/** @brief An asset file is selected, from the Asset Browser. */
struct asset_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()}; // nil for asset_kind::scene, which is never imported
  std::filesystem::path path{};               // project-relative to the active project's assets directory
  asset_kind kind{asset_kind::unknown};
}; // struct asset_selection

using selection = std::variant<empty_selection, node_selection, asset_selection>;

/** @brief Which handles the viewport gizmo shows for the current selection. */
enum class gizmo_operation {
  translate,
  rotate,
  scale,
}; // enum class gizmo_operation

/** @brief Whether the gizmo manipulates in the selected node's local axes or world axes. */
enum class gizmo_mode {
  local,
  world,
}; // enum class gizmo_mode

/**
 * @brief State shared across editor panels: what's currently selected (a node or an asset), plus
 * the viewport gizmo's operation/mode. Anything private to a single panel lives on that panel
 * instead (see editor_panel) — this only holds what genuinely crosses panel boundaries.
 */
struct editor_state {

  selection current_selection{empty_selection{}};
  gizmo_operation current_gizmo_operation{gizmo_operation::translate};
  gizmo_mode current_gizmo_mode{gizmo_mode::world};

  /**
   * @brief Re-resolves the currently selected node (if any) against @p scene, via its uuid.
   * Returns an invalid node if nothing is selected, the selection isn't a node, or no node with
   * that uuid exists any more (e.g. it was deleted).
   */
  [[nodiscard]] auto selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node;

  [[nodiscard]] auto is_node_selected(const sbx::scenes::node& node) const noexcept -> bool;

  auto select_node(const sbx::scenes::node& node) -> void;

  auto select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void;

  auto clear_selection() -> void;

}; // struct editor_state

} // namespace editor

#endif // EDITOR_EDITOR_STATE_HPP_
