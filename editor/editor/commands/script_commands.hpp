// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_
#define EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_

#include <string>
#include <utility>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/** @brief Attaches a new script entry without overrides; in Edit mode a pure data change. */
class attach_script_command final : public command {

public:

  attach_script_command(sbx::math::uuid node_id, std::string class_name)
  : _node_id{node_id}, _class_name{std::move(class_name)} { }

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Attach Script";
  }

private:

  sbx::math::uuid _node_id;
  std::string _class_name;

}; // class attach_script_command

/** @brief Detaches a script; construct it with the entry's current value first, so undo restores its overrides. */
class detach_script_command final : public command {

public:

  detach_script_command(sbx::math::uuid node_id, sbx::scenes::script_entry before)
  : _node_id{node_id}, _before{std::move(before)} { }

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Detach Script";
  }

private:

  sbx::math::uuid _node_id;
  sbx::scenes::script_entry _before;

}; // class detach_script_command

} // namespace editor

#endif // EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_
