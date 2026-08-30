// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/commands/command_stack.hpp>

namespace editor {

auto command_stack::push(std::unique_ptr<command> cmd) -> void {
  if (!cmd) {
    return;
  }

  cmd->execute();

  _redo_stack.clear();
  _undo_stack.push_back(std::move(cmd));

  if (_undo_stack.size() > max_history) {
    _undo_stack.erase(_undo_stack.begin());
  }
}

auto command_stack::undo() -> void {
  if (_undo_stack.empty()) {
    return;
  }

  auto cmd = std::move(_undo_stack.back());
  _undo_stack.pop_back();

  cmd->undo();

  _redo_stack.push_back(std::move(cmd));
}

auto command_stack::redo() -> void {
  if (_redo_stack.empty()) {
    return;
  }

  auto cmd = std::move(_redo_stack.back());
  _redo_stack.pop_back();

  cmd->execute();

  _undo_stack.push_back(std::move(cmd));
}

auto command_stack::undo_label() const -> std::string {
  return _undo_stack.empty() ? std::string{} : _undo_stack.back()->label();
}

auto command_stack::redo_label() const -> std::string {
  return _redo_stack.empty() ? std::string{} : _redo_stack.back()->label();
}

auto command_stack::clear() -> void {
  _undo_stack.clear();
  _redo_stack.clear();
}

} // namespace editor
