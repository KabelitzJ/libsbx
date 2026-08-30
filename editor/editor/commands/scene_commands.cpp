// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/commands/scene_commands.hpp>

#include <utility>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <editor/commands/scene_access.hpp>

namespace editor {

create_node_command::create_node_command(std::optional<sbx::math::uuid> parent_id, std::string name)
: _parent_id{parent_id}, _name{std::move(name)} { }

auto create_node_command::execute() -> void {
  if (_id == sbx::math::uuid::nil()) {
    _id = sbx::math::uuid::create();
  }

  auto& scene = active_scene();
  auto node = scene.create_node(_name, {}, _id);

  if (_parent_id) {
    if (auto parent = scene.find(*_parent_id); parent.is_valid()) {
      node.set_parent(parent);
    }
  }
}

auto create_node_command::undo() -> void {
  auto& scene = active_scene();
  scene.destroy_node(scene.find(_id));
}

delete_node_command::delete_node_command(sbx::scenes::node target)
: _id{target.id()} {
  auto& scene = active_scene();

  const auto& own_relationship = target.get_component<sbx::scenes::relationship>();
  auto parent_node = scene.node_of(own_relationship.parent);
  const auto parent_is_real = parent_node.has_component<sbx::scenes::id>();

  if (parent_is_real) {
    _parent_id = parent_node.id();
  }

  auto parent_or_root = parent_is_real ? parent_node : scene.root();
  const auto& siblings = parent_or_root.get_component<sbx::scenes::relationship>().children;

  for (auto i = std::size_t{0u}; i < siblings.size(); ++i) {
    if (scene.node_of(siblings[i]).id() == _id) {
      _index = i;
      break;
    }
  }

  _snapshot = sbx::scenes::scene_serializer::serialize_subtree(scene, target);

  // A descendant, not just target itself, can hold either binding — scene::destroy_node clears
  // both wherever they occur in the subtree, so undo needs to know which node(s) to restore them to.
  const auto check = [&](this const auto& self, sbx::scenes::node current) -> void {
    if (auto camera = scene.active_camera(); camera.is_valid() && camera.id() == current.id()) {
      _was_active_camera = current.id();
    }

    if (auto light = scene.primary_light(); light.is_valid() && light.id() == current.id()) {
      _was_primary_light = current.id();
    }

    for (const auto child : current.get_component<sbx::scenes::relationship>().children) {
      self(scene.node_of(child));
    }
  };

  check(target);
}

auto delete_node_command::execute() -> void {
  auto& scene = active_scene();
  scene.destroy_node(scene.find(_id));
}

auto delete_node_command::undo() -> void {
  auto& scene = active_scene();

  auto recreated = sbx::scenes::scene_serializer::deserialize_subtree(scene, _snapshot);
  auto parent_or_root = _parent_id ? scene.find(*_parent_id) : scene.root();

  if (!_parent_id || parent_or_root.is_valid()) {
    scene.insert_child(parent_or_root, recreated, _index);
  }

  if (_was_active_camera) {
    if (auto node = scene.find(*_was_active_camera); node.is_valid()) {
      scene.set_active_camera(node);
    }
  }

  if (_was_primary_light) {
    if (auto node = scene.find(*_was_primary_light); node.is_valid()) {
      scene.set_primary_light(node);
    }
  }
}

} // namespace editor
