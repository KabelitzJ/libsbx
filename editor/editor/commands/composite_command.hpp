// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMPOSITE_COMMAND_HPP_
#define EDITOR_COMMANDS_COMPOSITE_COMMAND_HPP_

#include <memory>
#include <string>
#include <vector>

#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/**
 * @brief Several commands as one undo step: execute() runs them in order, undo() in reverse. Sub-commands must be fully constructed first.
 *
 * Used for gestures affecting several nodes; a single command is pushed directly to keep its label.
 */
class composite_command final : public command {

public:

  composite_command(std::vector<std::unique_ptr<command>> commands, std::string label);

  auto execute(sbx::scenes::scene& target) -> void override;

  auto undo(sbx::scenes::scene& target) -> void override;

  [[nodiscard]] auto label() const -> std::string override {
    return _label;
  }

private:

  std::vector<std::unique_ptr<command>> _commands;
  std::string _label;

}; // class composite_command

} // namespace editor

#endif // EDITOR_COMMANDS_COMPOSITE_COMMAND_HPP_
