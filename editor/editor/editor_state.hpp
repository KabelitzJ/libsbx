// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_STATE_HPP_
#define EDITOR_EDITOR_STATE_HPP_

#include <filesystem>
#include <utility>
#include <variant>
#include <vector>

#include <libsbx/ecs/entity.hpp>

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

/** @brief A scene node is selected, identified by its (not necessarily stable across reload) entity. */
struct node_selection {
  sbx::ecs::entity entity{sbx::ecs::null_entity};
}; // struct node_selection

/** @brief An asset file is selected, from the Asset Browser. */
struct asset_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()}; // nil for asset_kind::scene, which is never imported
  std::filesystem::path path{};               // project-relative to the active project's assets directory
  asset_kind kind{asset_kind::unknown};
}; // struct asset_selection

using selection = std::variant<empty_selection, node_selection, asset_selection>;

/** @brief One row cached by the Asset Browser for the currently browsed directory. */
struct asset_browser_entry {
  std::filesystem::path path{}; // project-relative
  bool is_directory{false};
  asset_kind kind{asset_kind::unknown};
  bool is_importable{false};
  sbx::math::uuid id{sbx::math::uuid::nil()}; // resolved lazily, on click
}; // struct asset_browser_entry

/** @brief Asset Browser panel state: which directory is open and its cached listing. */
struct asset_browser_state {
  std::filesystem::path current_directory{}; // project-relative; empty = assets root
  std::vector<asset_browser_entry> cached_entries{};
  bool needs_refresh{true};
}; // struct asset_browser_state

/**
 * @brief Shared state passed to every editor panel: what's currently selected (a node or an
 * asset), plus per-panel state that doesn't belong anywhere else. Panels only ever read or write
 * through this — they never reference each other directly.
 */
struct editor_state {

  selection current_selection{empty_selection{}};
  asset_browser_state asset_browser{};

  /**
   * @brief Re-resolves the currently selected node (if any) against @p scene. Returns an invalid
   * node if nothing is selected, the selection isn't a node, or the entity is no longer alive.
   */
  [[nodiscard]] auto selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node;

  [[nodiscard]] auto is_node_selected(sbx::ecs::entity entity) const noexcept -> bool;

  auto select_node(sbx::ecs::entity entity) -> void;

  auto select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void;

  auto clear_selection() -> void;

}; // struct editor_state

} // namespace editor

#endif // EDITOR_EDITOR_STATE_HPP_
