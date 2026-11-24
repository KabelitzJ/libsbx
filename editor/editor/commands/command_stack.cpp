// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/commands/command_stack.hpp>

namespace editor {

auto command_stack::push(sbx::scenes::scene& target, std::unique_ptr<command> cmd) -> void {
  if (!cmd) {
    return;
  }

  cmd->execute(target);
  ++_revision;

  _redo_stack.clear();
  _undo_stack.push_back(std::move(cmd));

  if (_undo_stack.size() > max_history) {
    _undo_stack.erase(_undo_stack.begin());
  }
}

auto command_stack::undo(sbx::scenes::scene& target) -> void {
  if (_undo_stack.empty()) {
    return;
  }

  auto cmd = std::move(_undo_stack.back());
  _undo_stack.pop_back();

  cmd->undo(target);
  ++_revision;

  _redo_stack.push_back(std::move(cmd));
}

auto command_stack::redo(sbx::scenes::scene& target) -> void {
  if (_redo_stack.empty()) {
    return;
  }

  auto cmd = std::move(_redo_stack.back());
  _redo_stack.pop_back();

  cmd->execute(target);
  ++_revision;

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
  ++_revision;
}

auto command_stack::history(std::size_t& applied_count) const -> std::vector<std::string> {
  auto labels = std::vector<std::string>{};
  labels.reserve(_undo_stack.size() + _redo_stack.size());

  for (const auto& entry : _undo_stack) {
    labels.push_back(entry->label());
  }

  // The redo stack's back is the next redo.
  for (auto entry = _redo_stack.rbegin(); entry != _redo_stack.rend(); ++entry) {
    labels.push_back((*entry)->label());
  }

  applied_count = _undo_stack.size();

  return labels;
}

} // namespace editor
