// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMMAND_HPP_
#define EDITOR_COMMANDS_COMMAND_HPP_

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

namespace editor {

/**
 * @brief One undoable editor action.
 *
 * Construct without mutating, then push via editor_state::push_command() (or
 * command_stack::push()), which calls execute() — never call execute()/undo() directly.
 *
 * Stores only math::uuids, never scene&/ecs::entity; re-resolves via target.find() inside
 * execute()/undo(), treating a lookup miss as a no-op. target is passed in explicitly by the
 * caller (editor_state::push_command()/undo()/redo(), see its doc comment) rather than resolved
 * internally, so the exact same command classes work unmodified against any scene a caller wants
 * to target — normally editor::active_scene(), but e.g. inspector_panel's prefab edit session
 * passes its own private scratch scene instead. Never storing target itself also keeps a command
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

  /** @brief Performs the mutation against target. Called once when first pushed, and again on redo. */
  virtual auto execute(sbx::scenes::scene& target) -> void = 0;

  virtual auto undo(sbx::scenes::scene& target) -> void = 0;

  /** @brief Short description for the Edit menu, e.g. "Create Node" -> "Undo Create Node". */
  [[nodiscard]] virtual auto label() const -> std::string = 0;

  /**
   * @brief Commands that repeat this edit on each of nodes -- editor_state::push_command asks while several nodes are selected
   * in the Inspector (see editor_state::broadcast_targets) and pushes them together with this one as a single undo step. Called
   * before execute(). Default: none, the edit only ever affects its own node.
   */
  [[nodiscard]] virtual auto broadcast([[maybe_unused]] sbx::scenes::scene& target, [[maybe_unused]] std::span<const sbx::math::uuid> nodes) const -> std::vector<std::unique_ptr<command>> {
    return {};
  }

}; // class command

} // namespace editor

#endif // EDITOR_COMMANDS_COMMAND_HPP_
