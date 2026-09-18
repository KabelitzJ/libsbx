// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
#define LIBSBX_SCENES_SCENE_SERIALIZER_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/prefab.hpp>

namespace YAML { class Node; } // avoids pulling yaml-cpp's full header into every scene_serializer.hpp include -- moot for prefab_handle, already transitively complete via scene.hpp -> components.hpp -> assets/prefab.hpp, but harmless to leave in place for the rest of this header's YAML::Node-by-reference uses

namespace sbx::scenes {

class scene_serializer final {

public:

  scene_serializer() = delete;

  /** @brief Builds target's full YAML snapshot ({metadata, assets_module, nodes}), without touching disk -- what save()/serialize() write, and what assets_module::create_scene/save_scene need to hand a scene to the asset system. */
  [[nodiscard]] static auto build(scene& target) -> YAML::Node;

  static auto save(scene& target, const std::filesystem::path& path) -> void;

  /** @brief Renders target to the same YAML save() would write, without touching disk. */
  [[nodiscard]] static auto serialize(scene& target) -> std::string;

  static auto load(scene& target, const std::filesystem::path& path) -> void;

  /** @brief Applies snapshot directly to target -- same effect as load(target, path) minus the file read, for a snapshot already in memory (e.g. assets::scene_handle::snapshot()). */
  static auto load(scene& target, const YAML::Node& snapshot) -> void;

  /** @brief Snapshots subtree_root and its whole descendant subtree — components, structure, ids — self-contained (own asset table), for undo/redo. */
  [[nodiscard]] static auto serialize_subtree(scene& target, node subtree_root) -> YAML::Node;

  /** @brief Recreates a serialize_subtree() snapshot under target._root; caller repositions it (see scene::insert_child). Returns the recreated root. */
  static auto deserialize_subtree(scene& target, const YAML::Node& snapshot) -> node;

  /** @brief serialize_subtree(source, subtree_root) wrapped straight into a new, unsaved prefab asset. The editor's "Create Prefab..." entry point (the caller still needs assets_module::save_prefab to persist it). */
  [[nodiscard]] static auto create_prefab_from_node(scene& source, node subtree_root, std::string name) -> assets::prefab_handle;

  /**
   * @brief Tags subtree_root and its whole descendant subtree as an instance of prefab, without
   * touching any component — used right after create_prefab_from_node, when the live subtree's
   * content already matches the prefab exactly (its ids are the prefab's own member ids, since
   * create_prefab_from_node didn't remap anything).
   */
  static auto attach_prefab_instance(scene& target, node subtree_root, assets::prefab_handle prefab) -> void;

  /**
   * @brief Instantiates prefab's current snapshot into target as a new subtree. Mints a fresh
   * scenes::id for every node (root_id, if given, is forced onto the root instead — used by
   * sync_prefab_instances to keep an existing instance's root identity across a resync) and tags
   * every created node with prefab_member{<its id in the prefab's own snapshot>}. The new subtree
   * is left attached under target's root — the caller repositions it (see scene::insert_child).
   */
  static auto instantiate_prefab(scene& target, const assets::prefab_handle& prefab, std::optional<math::uuid> root_id = std::nullopt) -> node;

  /**
   * @brief Resyncs every prefab_instance in target whose source has moved past applied_generation:
   * per-node, per-component merges the prefab's current snapshot onto the live instance, skipping
   * any (member, component) pair the instance has overridden — see prefab_override's doc comment.
   * Call once per editor frame and once after a scene loads.
   */
  static auto sync_prefab_instances(scene& target) -> void;

  /**
   * @brief Pushes source_node's current component_key component into its owning instance's prefab
   * (an ancestor-or-self walk finds the prefab_instance root), persists it (assets_module::
   * update_prefab + save_prefab), and clears the matching override — other instances without their
   * own override on that key pick the change up on their next sync_prefab_instances pass. No-op if
   * source_node isn't part of a prefab instance.
   */
  static auto apply_prefab_override(scene& target, node source_node, std::string_view component_key) -> void;

  /**
   * @brief Overwrites target_node's component_key component with its prefab's current value for
   * that node and clears the matching override. No-op if target_node isn't part of a prefab
   * instance, or its prefab has no such component for it.
   */
  static auto revert_prefab_override(scene& target, node target_node, std::string_view component_key) -> void;

  /** @brief member_node's own component_value/component_removed overrides (never node_removed, which has no meaningful component_key) — the Hierarchy/Inspector's "Apply to Prefab"/"Revert to Prefab" menus list these. Empty if member_node isn't part of a prefab instance. */
  [[nodiscard]] static auto prefab_overrides_of(scene& target, node member_node) -> std::vector<prefab_override>;

  /**
   * @brief Upserts (member_node's instance, member_node's member_id, component_key) -> kind into
   * the owning instance's override list — the recording half of the override system a component
   * edit needs to go through (see editor::mark_prefab_override, the per-Component-typed wrapper
   * every property-edit command actually calls). component_key is ignored (pass "") for
   * node_removed. No-op if member_node isn't part of a prefab instance.
   */
  static auto mark_prefab_override(scene& target, node member_node, std::string_view component_key, prefab_override_kind kind) -> void;

  /**
   * @brief Pushes instance_root's entire current subtree back to its prefab in one go (not a
   * single component_key like apply_prefab_override — everything), persists it, then clears this
   * instance's whole override list and marks it caught up (it now trivially matches the prefab by
   * definition). Every other instance still resyncs/keeps its own overrides exactly as before,
   * through the normal sync_prefab_instances path. No-op if instance_root isn't a prefab instance.
   */
  static auto update_prefab_from_node(scene& source, node instance_root) -> void;

}; // class scene_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
