// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMMAND_STACK_HPP_
#define EDITOR_COMMANDS_COMMAND_STACK_HPP_

#include <memory>
#include <string>
#include <vector>

#include <editor/commands/command.hpp>

namespace editor {

/**
 * @brief Standard undo/redo stack of editor commands. Pushing a new command always clears the
 * redo stack (the usual convention: once you do something new, the branch of history you undid
 * away from is gone).
 */
class command_stack final {

public:

  /** @brief No-op if cmd is null; otherwise runs cmd->execute(), clears the redo stack, and pushes cmd onto the undo stack (dropping the oldest entry past max_history). */
  auto push(std::unique_ptr<command> cmd) -> void;

  /** @brief No-op if there's nothing to undo. */
  auto undo() -> void;

  /** @brief No-op if there's nothing to redo. */
  auto redo() -> void;

  [[nodiscard]] auto can_undo() const noexcept -> bool {
    return !_undo_stack.empty();
  }

  [[nodiscard]] auto can_redo() const noexcept -> bool {
    return !_redo_stack.empty();
  }

  /** @brief The top-of-undo-stack command's label(), or "" if can_undo() is false. */
  [[nodiscard]] auto undo_label() const -> std::string;

  /** @brief The top-of-redo-stack command's label(), or "" if can_redo() is false. */
  [[nodiscard]] auto redo_label() const -> std::string;

  /** @brief Drops all history — call whenever previously-pushed commands can no longer be safely replayed (see editor_module::exit_play_mode()). */
  auto clear() -> void;

private:

  inline static constexpr auto max_history = std::size_t{100u};

  std::vector<std::unique_ptr<command>> _undo_stack{};
  std::vector<std::unique_ptr<command>> _redo_stack{};

}; // class command_stack

} // namespace editor

#endif // EDITOR_COMMANDS_COMMAND_STACK_HPP_
