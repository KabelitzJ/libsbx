// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMMAND_STACK_HPP_
#define EDITOR_COMMANDS_COMMAND_STACK_HPP_

#include <memory>
#include <string>
#include <vector>

#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>

namespace editor {

/** @brief The undo/redo stack; pushing clears the redo branch. The target scene is passed on every call, never stored. */
class command_stack final {

public:

  /**
   * @brief Executes @p cmd, clears the redo stack and pushes it, dropping the oldest entry past max_history. No-op for null.
   *
   * @param target The scene.
   * @param cmd The command.
   */
  auto push(sbx::scenes::scene& target, std::unique_ptr<command> cmd) -> void;

  /**
   * @brief Undoes the last command, if any.
   *
   * @param target The scene.
   */
  auto undo(sbx::scenes::scene& target) -> void;

  /**
   * @brief Redoes the last undone command, if any.
   *
   * @param target The scene.
   */
  auto redo(sbx::scenes::scene& target) -> void;

  [[nodiscard]] auto can_undo() const noexcept -> bool {
    return !_undo_stack.empty();
  }

  [[nodiscard]] auto can_redo() const noexcept -> bool {
    return !_redo_stack.empty();
  }

  /**
   * @brief The label of the command undo() would revert.
   *
   * @return The label, or empty if there is nothing to undo.
   */
  [[nodiscard]] auto undo_label() const -> std::string;

  /**
   * @brief The label of the command redo() would reapply.
   *
   * @return The label, or empty if there is nothing to redo.
   */
  [[nodiscard]] auto redo_label() const -> std::string;

  /** @brief Drops all history, whenever commands can no longer be replayed (e.g. leaving play mode). */
  auto clear() -> void;

private:

  inline static constexpr auto max_history = std::size_t{100u};

  std::vector<std::unique_ptr<command>> _undo_stack{};
  std::vector<std::unique_ptr<command>> _redo_stack{};

}; // class command_stack

} // namespace editor

#endif // EDITOR_COMMANDS_COMMAND_STACK_HPP_
