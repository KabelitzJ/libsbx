// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCENE_COMMANDS_HPP_
#define EDITOR_COMMANDS_SCENE_COMMANDS_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/** @brief Creates one new node (no children), optionally parented under parent_id. */
class create_node_command final : public command {

public:

  explicit create_node_command(std::optional<sbx::math::uuid> parent_id = std::nullopt, std::string name = "Node");

  auto execute() -> void override;

  auto undo() -> void override;

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

/**
 * @brief Deletes target and its whole subtree. Snapshots everything undo needs to restore it —
 * components, structure, ids, sibling position, and any active-camera/primary-light binding — at
 * construction time, before anything is actually deleted.
 */
class delete_node_command final : public command {

public:

  explicit delete_node_command(const sbx::scenes::node& target);

  auto execute() -> void override;

  auto undo() -> void override;

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

} // namespace editor

#endif // EDITOR_COMMANDS_SCENE_COMMANDS_HPP_
