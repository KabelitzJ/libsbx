// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMMAND_HPP_
#define EDITOR_COMMANDS_COMMAND_HPP_

#include <string>

namespace editor {

/**
 * @brief One undoable editor action.
 *
 * Construct without mutating, then push via editor_state::push_command() (or
 * command_stack::push()), which calls execute() — never call execute()/undo() directly.
 *
 * Stores only math::uuids, never scene&/ecs::entity; re-resolves via scene::find() inside
 * execute()/undo() (@ref active_scene), treating a lookup miss as a no-op. This keeps a command
 * valid across the registry rebuild scene_serializer::load() does on Play -> Stop.
 *
 * modify_component_command/add_component_command are idempotent (plain overwrite /
 * get_or_add_component), so a continuous drag can re-execute every frame. Node/script commands
 * are strict alternating toggles instead, since command_stack never calls execute()/undo() twice
 * in a row on the same state.
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
