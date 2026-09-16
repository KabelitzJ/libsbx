// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/commands/composite_command.hpp>

#include <utility>

namespace editor {

composite_command::composite_command(std::vector<std::unique_ptr<command>> commands, std::string label)
: _commands{std::move(commands)}, _label{std::move(label)} { }

auto composite_command::execute(sbx::scenes::scene& target) -> void {
  for (auto& sub_command : _commands) {
    sub_command->execute(target);
  }
}

auto composite_command::undo(sbx::scenes::scene& target) -> void {
  for (auto entry = _commands.rbegin(); entry != _commands.rend(); ++entry) {
    (*entry)->undo(target);
  }
}

} // namespace editor
