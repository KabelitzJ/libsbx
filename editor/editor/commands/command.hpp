// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMMAND_HPP_
#define EDITOR_COMMANDS_COMMAND_HPP_

#include <string>

namespace editor {

/**
 * @brief One undoable editor action. Construct a command that hasn't mutated anything yet, then
 * hand it to editor_state::push_command() (or command_stack::push()), which calls execute() —
 * never call execute()/undo() directly.
 *
 * Every command stores only math::uuids, never scene&/ecs::entity — it re-resolves via
 * scene::find() inside execute()/undo() (re-fetching the active scene itself, see
 * commands/scene_access.hpp), tolerating a lookup miss as a safe no-op. This is what keeps a
 * command safe to hold across the full registry rebuild scene_serializer::load() does on
 * Play -> Stop.
 *
 * modify_component_command/add_component_command are naturally idempotent (plain overwrite /
 * get_or_add_component) — execute() re-applying an already-current value is harmless, which is
 * what lets a continuous drag edit keep writing live every frame and only push one command at the
 * end. create_node_command/delete_node_command/attach_script_command/detach_script_command are
 * strictly-alternated toggles instead: command_stack's own push/undo/redo contract never calls
 * either twice in a row against the same state, so they don't need extra idempotency guards.
 */
class command {

public:

  virtual ~command() = default;

  /** @brief Performs the mutation. Called once when first pushed, and again on redo. */
  virtual auto execute() -> void = 0;

  virtual auto undo() -> void = 0;

  /** @brief Short description for the Edit menu, e.g. "Create Node" -> "Undo Create Node". */
  [[nodiscard]] virtual auto label() const -> std::string = 0;

}; // class command

} // namespace editor

#endif // EDITOR_COMMANDS_COMMAND_HPP_
