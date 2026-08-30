// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_
#define EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_

#include <string>
#include <utility>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/components.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/** @brief Attaches a fresh, override-less script_entry for class_name. In Edit mode (the realistic invocation surface) this is a pure data mutation — no managed-runtime side effects. */
class attach_script_command final : public command {

public:

  attach_script_command(sbx::math::uuid node_id, std::string class_name)
  : _node_id{node_id}, _class_name{std::move(class_name)} { }

  auto execute() -> void override;

  auto undo() -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Attach Script";
  }

private:

  sbx::math::uuid _node_id;
  std::string _class_name;

}; // class attach_script_command

/**
 * @brief Detaches a script. Construct this with the entry's current value (class name + field
 * overrides) BEFORE detaching — undo restores that exact entry, not a fresh override-less one.
 */
class detach_script_command final : public command {

public:

  detach_script_command(sbx::math::uuid node_id, sbx::scenes::script_entry before)
  : _node_id{node_id}, _before{std::move(before)} { }

  auto execute() -> void override;

  auto undo() -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return "Detach Script";
  }

private:

  sbx::math::uuid _node_id;
  sbx::scenes::script_entry _before;

}; // class detach_script_command

} // namespace editor

#endif // EDITOR_COMMANDS_SCRIPT_COMMANDS_HPP_
