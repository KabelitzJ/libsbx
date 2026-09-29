// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCENE_COMMANDS_HPP_
#define EDITOR_COMMANDS_SCENE_COMMANDS_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/prefab.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/** @brief Creates one new node (no children), optionally parented under parent_id. */
class create_node_command final : public command {

public:

  explicit create_node_command(std::optional<sbx::math::uuid> parent_id = std::nullopt, std::string name = "Node");

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Create Node";
  }

  /** @brief The created node's id — valid to read right after command_stack::push() returns. */
  [[nodiscard]] auto id() const noexcept -> sbx::math::uuid {
    return _id;
  }

private:

  std::optional<sbx::math::uuid> _parent_id;
  std::string _name;
  sbx::math::uuid _id{sbx::math::uuid::nil()};

}; // class create_node_command

/** @brief Creates one new node named name, at local position, with a mesh_renderer pointed at mesh_id — the Hierarchy panel's "Create > 3D Object" menu (built-in primitives) and mesh assets dropped into the viewport. */
class create_mesh_node_command final : public command {

public:

  create_mesh_node_command(sbx::math::uuid mesh_id, std::string name, std::optional<sbx::math::uuid> parent_id = std::nullopt, sbx::math::vector3 position = sbx::math::vector3{0.0f, 0.0f, 0.0f});

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Create " + _name;
  }

  /** @brief The created node's id — valid to read right after command_stack::push() returns. */
  [[nodiscard]] auto id() const noexcept -> sbx::math::uuid {
    return _id;
  }

private:

  sbx::math::uuid _mesh_id;
  std::string _name;
  std::optional<sbx::math::uuid> _parent_id;
  sbx::math::vector3 _position;
  sbx::math::uuid _id{sbx::math::uuid::nil()};

}; // class create_mesh_node_command

/** @brief Instantiates prefab as a new subtree, optionally parented under parent_id and moved to local position — the Hierarchy panel's and the viewport's prefab drag-drop. */
class instantiate_prefab_command final : public command {

public:

  explicit instantiate_prefab_command(sbx::assets::prefab_handle prefab, std::optional<sbx::math::uuid> parent_id = std::nullopt, std::optional<sbx::math::vector3> position = std::nullopt);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Instantiate " + (_prefab.is_valid() ? _prefab->name() : std::string{"Prefab"});
  }

  /** @brief The created instance's root id — valid to read right after command_stack::push() returns. */
  [[nodiscard]] auto id() const noexcept -> sbx::math::uuid {
    return _id;
  }

private:

  sbx::assets::prefab_handle _prefab;
  std::optional<sbx::math::uuid> _parent_id;
  std::optional<sbx::math::vector3> _position;
  sbx::math::uuid _id{sbx::math::uuid::nil()};

}; // class instantiate_prefab_command

/**
 * @brief Recreates a serialize_subtree() snapshot at index among parent_id's children (nullopt = top-level). The snapshot's ids
 * are used as-is, so redo recreates the exact same nodes -- callers pass it through scene_serializer::with_fresh_ids first.
 * Backs Duplicate and Paste.
 */
class insert_subtree_command final : public command {

public:

  insert_subtree_command(YAML::Node snapshot, std::optional<sbx::math::uuid> parent_id, std::size_t index, std::string label);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return _label;
  }

  [[nodiscard]] auto id() const -> sbx::math::uuid {
    return _snapshot["nodes"][0]["id"].as<sbx::math::uuid>();
  }

private:

  YAML::Node _snapshot;
  std::optional<sbx::math::uuid> _parent_id;
  std::size_t _index;
  std::string _label;

}; // class insert_subtree_command

/**
 * @brief Deletes target and its whole subtree. Snapshots everything undo needs to restore it —
 * components, structure, ids, sibling position, and any active-camera/primary-light binding — at
 * construction time, before anything is actually deleted.
 */
class delete_node_command final : public command {

public:

  explicit delete_node_command(sbx::scenes::scene& scene, const sbx::scenes::node& target);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Delete Node";
  }

private:

  sbx::math::uuid _id;
  std::optional<sbx::math::uuid> _parent_id{}; // nullopt = was top-level
  std::size_t _index{0u};
  YAML::Node _snapshot;
  std::optional<sbx::math::uuid> _was_active_camera{};
  std::optional<sbx::math::uuid> _was_primary_light{};

}; // class delete_node_command

/**
 * @brief Moves target to a new parent (nullopt = top-level) at new_index among that parent's
 * children, restoring its original parent/index on undo. Backs the Hierarchy panel's drag/drop
 * reparent and reorder.
 *
 * new_index is relative to the destination list *before* target is removed from wherever it
 * currently sits — execute() corrects for the shift itself when target is moving within the same
 * parent, so callers just pass the raw drop-target position.
 */
class reparent_node_command final : public command {

public:

  explicit reparent_node_command(sbx::scenes::scene& scene, const sbx::scenes::node& target, std::optional<sbx::math::uuid> new_parent_id, std::size_t new_index);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Move Node";
  }

private:

  sbx::math::uuid _id;
  std::optional<sbx::math::uuid> _old_parent_id{}; // nullopt = was top-level
  std::size_t _old_index{0u};
  std::optional<sbx::math::uuid> _new_parent_id;
  std::size_t _new_index;

}; // class reparent_node_command

/** @brief Sets the scene's active (play) camera to target, restoring whatever it was before on undo. */
class set_active_camera_command final : public command {

public:

  explicit set_active_camera_command(sbx::scenes::scene& scene, const sbx::scenes::node& target);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Set Active Camera";
  }

private:

  sbx::math::uuid _id;
  std::optional<sbx::math::uuid> _previous_id{}; // nullopt = there was no active camera before

}; // class set_active_camera_command

} // namespace editor

#endif // EDITOR_COMMANDS_SCENE_COMMANDS_HPP_
