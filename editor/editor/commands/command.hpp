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
 * @brief One undoable editor action. Construct without mutating and push through editor_state::push_command(), which calls execute().
 *
 * Commands store only uuids and re-resolve them in the scene passed to execute()/undo(), treating misses as no-ops.
 * That keeps them valid across the registry rebuild on Play -> Stop and lets them target any scene, such as the prefab edit scratch scene.
 *
 * modify_component_command and add_component_command are idempotent, so a drag can re-execute every frame; node and script commands strictly alternate.
 */
class command {

public:

  virtual ~command() = default;

  /**
   * @brief Performs the change; called when first pushed and again on redo.
   *
   * @param target The scene to change.
   */
  virtual auto execute(sbx::scenes::scene& target) -> void = 0;

  /**
   * @brief Reverts the change made by execute().
   *
   * @param target The scene to change.
   */
  virtual auto undo(sbx::scenes::scene& target) -> void = 0;

  /**
   * @brief A short description for the Edit menu, e.g. "Create Node".
   *
   * @return The label.
   */
  [[nodiscard]] virtual auto label() const -> std::string = 0;

  /**
   * @brief Commands repeating this edit on each of @p nodes, pushed with this one as a single undo step while the Inspector edits a multi-selection. Called before execute().
   *
   * @param target The scene.
   * @param nodes The other selected nodes.
   *
   * @return The extra commands; none by default.
   */
  [[nodiscard]] virtual auto broadcast([[maybe_unused]] sbx::scenes::scene& target, [[maybe_unused]] std::span<const sbx::math::uuid> nodes) const -> std::vector<std::unique_ptr<command>> {
    return {};
  }

}; // class command

} // namespace editor

#endif // EDITOR_COMMANDS_COMMAND_HPP_
