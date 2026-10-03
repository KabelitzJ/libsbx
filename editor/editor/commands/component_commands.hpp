// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_COMPONENT_COMMANDS_HPP_
#define EDITOR_COMMANDS_COMPONENT_COMMANDS_HPP_

#include <string>
#include <tuple>
#include <utility>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>
#include <editor/commands/field_diff.hpp>
#include <editor/commands/prefab_override.hpp>

namespace editor {

/** @brief Adds a default-constructed Component; execute() and undo() are no-ops when already in the target state. */
template<typename Component>
class add_component_command final : public command {

public:

  add_component_command(sbx::math::uuid node_id, std::string label)
  : _node_id{node_id}, _label{std::move(label)} { }

  auto execute(sbx::scenes::scene& target) -> void override {
    if (auto node = target.find(_node_id); node.is_valid()) {
      node.get_or_add_component<Component>();
      mark_prefab_override<Component>(target, node, sbx::scenes::prefab_override_kind::component_value);
    }
  }

  auto undo(sbx::scenes::scene& target) -> void override {
    if (auto node = target.find(_node_id); node.is_valid()) {
      node.remove_component<Component>();
    }
  }

  [[nodiscard]] auto label() const -> std::string override {
    return _label;
  }

private:

  sbx::math::uuid _node_id;
  std::string _label;

}; // class add_component_command

/** @brief Removes a Component; construct it before removing, passing the current value as @p before. */
template<typename Component>
class remove_component_command final : public command {

public:

  remove_component_command(sbx::math::uuid node_id, Component before, std::string label)
  : _node_id{node_id}, _before{std::move(before)}, _label{std::move(label)} { }

  auto execute(sbx::scenes::scene& target) -> void override {
    if (auto node = target.find(_node_id); node.is_valid()) {
      node.remove_component<Component>();
      mark_prefab_override<Component>(target, node, sbx::scenes::prefab_override_kind::component_removed);
    }
  }

  auto undo(sbx::scenes::scene& target) -> void override {
    if (auto node = target.find(_node_id); node.is_valid()) {
      node.add_component<Component>(_before);
    }
  }

  [[nodiscard]] auto label() const -> std::string override {
    return _label;
  }

private:

  sbx::math::uuid _node_id;
  Component _before;
  std::string _label;

}; // class remove_component_command

/** @brief Overwrites a node's Component with @p after, restorable to @p before; the general property-edit command, snapshotting the whole component. */
template<typename Component>
class modify_component_command final : public command {

public:

  modify_component_command(sbx::math::uuid node_id, Component before, Component after, std::string label)
  : _node_id{node_id}, _before{std::move(before)}, _after{std::move(after)}, _label{std::move(label)} { }

  auto execute(sbx::scenes::scene& target) -> void override {
    _apply(target, _after);
  }

  auto undo(sbx::scenes::scene& target) -> void override {
    _apply(target, _before);
  }

  [[nodiscard]] auto label() const -> std::string override {
    return _label;
  }

  // Each other node with the Component gets only the fields this edit changed, on top of its own value.
  [[nodiscard]] auto broadcast(sbx::scenes::scene& target, std::span<const sbx::math::uuid> nodes) const -> std::vector<std::unique_ptr<command>> override {
    auto commands = std::vector<std::unique_ptr<command>>{};

    for (const auto id : nodes) {
      auto node = target.find(id);

      if (id == _node_id || !node.is_valid() || !node.has_component<Component>()) {
        continue;
      }

      const auto& before = node.get_component<Component>();
      auto after = before;

      apply_changed_fields(_before, _after, after);

      commands.push_back(std::make_unique<modify_component_command>(id, before, std::move(after), _label));
    }

    return commands;
  }

private:

  auto _apply(sbx::scenes::scene& target, const Component& value) -> void {
    if (auto node = target.find(_node_id); node.is_valid()) {
      node.get_or_add_component<Component>() = value;
      mark_prefab_override<Component>(target, node, sbx::scenes::prefab_override_kind::component_value);
    }
  }

  sbx::math::uuid _node_id;
  Component _before;
  Component _after;
  std::string _label;

}; // class modify_component_command

} // namespace editor

#endif // EDITOR_COMMANDS_COMPONENT_COMMANDS_HPP_
