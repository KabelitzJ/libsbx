// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/commands/scene_commands.hpp>

#include <utility>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace editor {

create_node_command::create_node_command(std::optional<sbx::math::uuid> parent_id, std::string name)
: _parent_id{parent_id}, _name{std::move(name)} { }

auto create_node_command::execute(sbx::scenes::scene& target) -> void {
  if (_id == sbx::math::uuid::nil()) {
    _id = sbx::math::uuid::create();
  }

  auto node = target.create_node(_name, {}, _id);

  if (_parent_id) {
    if (auto parent = target.find(*_parent_id); parent.is_valid()) {
      node.set_parent(parent);
    }
  }
}

auto create_node_command::undo(sbx::scenes::scene& target) -> void {
  target.destroy_node(target.find(_id));
}

create_mesh_node_command::create_mesh_node_command(sbx::math::uuid mesh_id, std::string name, std::optional<sbx::math::uuid> parent_id, sbx::math::vector3 position)
: _mesh_id{mesh_id}, _name{std::move(name)}, _parent_id{parent_id}, _position{position} { }

auto create_mesh_node_command::execute(sbx::scenes::scene& target) -> void {
  if (_id == sbx::math::uuid::nil()) {
    _id = sbx::math::uuid::create();
  }

  auto node = target.create_node(_name, {}, _id);
  node.transform().position = _position;

  if (_parent_id) {
    if (auto parent = target.find(*_parent_id); parent.is_valid()) {
      node.set_parent(parent);
    }
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto renderer = sbx::scenes::mesh_renderer{};
  renderer.mesh = assets_module.load_mesh(_mesh_id);
  sbx::scenes::sync_materials_with_mesh(renderer);

  node.add_component<sbx::scenes::mesh_renderer>(std::move(renderer));
}

auto create_mesh_node_command::undo(sbx::scenes::scene& target) -> void {
  target.destroy_node(target.find(_id));
}

instantiate_prefab_command::instantiate_prefab_command(sbx::assets::prefab_handle prefab, std::optional<sbx::math::uuid> parent_id, std::optional<sbx::math::vector3> position)
: _prefab{std::move(prefab)}, _parent_id{parent_id}, _position{position} { }

auto instantiate_prefab_command::execute(sbx::scenes::scene& target) -> void {
  auto instance = sbx::scenes::scene_serializer::instantiate_prefab(target, _prefab, (_id == sbx::math::uuid::nil()) ? std::nullopt : std::optional{_id});

  if (!instance.is_valid()) {
    return;
  }

  _id = instance.id();

  if (_position) {
    instance.transform().position = *_position;
  }

  if (_parent_id) {
    if (auto parent = target.find(*_parent_id); parent.is_valid()) {
      instance.set_parent(parent);
    }
  }
}

auto instantiate_prefab_command::undo(sbx::scenes::scene& target) -> void {
  target.destroy_node(target.find(_id));
}

insert_subtree_command::insert_subtree_command(YAML::Node snapshot, std::optional<sbx::math::uuid> parent_id, std::size_t index, std::string label)
: _snapshot{std::move(snapshot)}, _parent_id{parent_id}, _index{index}, _label{std::move(label)} { }

auto insert_subtree_command::execute(sbx::scenes::scene& target) -> void {
  auto created = sbx::scenes::scene_serializer::deserialize_subtree(target, _snapshot);
  auto parent_or_root = _parent_id ? target.find(*_parent_id) : target.root();

  if (!_parent_id || parent_or_root.is_valid()) {
    target.insert_child(parent_or_root, created, _index);
  }
}

auto insert_subtree_command::undo(sbx::scenes::scene& target) -> void {
  target.destroy_node(target.find(id()));
}

delete_node_command::delete_node_command(sbx::scenes::scene& scene, const sbx::scenes::node& target)
: _id{target.id()} {
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
  // Same walk also marks every prefab_member in the subtree as node_removed on its owning
  // instance — not just target itself — so sync_prefab_instances never resurrects a descendant
  // whose parent (target, or one of target's own descendants) got deleted along with it.
  const auto check = [&](this const auto& self, const sbx::scenes::node& current) -> void {
    if (auto camera = scene.active_camera(); camera.is_valid() && camera.id() == current.id()) {
      _was_active_camera = current.id();
    }

    if (auto light = scene.primary_light(); light.is_valid() && light.id() == current.id()) {
      _was_primary_light = current.id();
    }

    if (current.has_component<sbx::scenes::prefab_member>()) {
      sbx::scenes::scene_serializer::mark_prefab_override(scene, current, "", sbx::scenes::prefab_override_kind::node_removed);
    }

    for (const auto child : current.get_component<sbx::scenes::relationship>().children) {
      self(scene.node_of(child));
    }
  };

  check(target);
}

auto delete_node_command::execute(sbx::scenes::scene& target) -> void {
  target.destroy_node(target.find(_id));
}

auto delete_node_command::undo(sbx::scenes::scene& target) -> void {
  auto recreated = sbx::scenes::scene_serializer::deserialize_subtree(target, _snapshot);
  auto parent_or_root = _parent_id ? target.find(*_parent_id) : target.root();

  if (!_parent_id || parent_or_root.is_valid()) {
    target.insert_child(parent_or_root, recreated, _index);
  }

  if (_was_active_camera) {
    if (auto node = target.find(*_was_active_camera); node.is_valid()) {
      target.set_active_camera(node);
    }
  }

  if (_was_primary_light) {
    if (auto node = target.find(*_was_primary_light); node.is_valid()) {
      target.set_primary_light(node);
    }
  }
}

reparent_node_command::reparent_node_command(sbx::scenes::scene& scene, const sbx::scenes::node& target, std::optional<sbx::math::uuid> new_parent_id, std::size_t new_index)
: _id{target.id()}, _new_parent_id{new_parent_id}, _new_index{new_index} {
  const auto& own_relationship = target.get_component<sbx::scenes::relationship>();
  auto parent_node = scene.node_of(own_relationship.parent);
  const auto parent_is_real = parent_node.has_component<sbx::scenes::id>();

  if (parent_is_real) {
    _old_parent_id = parent_node.id();
  }

  auto parent_or_root = parent_is_real ? parent_node : scene.root();
  const auto& siblings = parent_or_root.get_component<sbx::scenes::relationship>().children;

  for (auto i = std::size_t{0u}; i < siblings.size(); ++i) {
    if (scene.node_of(siblings[i]).id() == _id) {
      _old_index = i;
      break;
    }
  }
}

auto reparent_node_command::execute(sbx::scenes::scene& target) -> void {
  auto node = target.find(_id);

  if (!node.is_valid()) {
    return;
  }

  auto index = _new_index;

  // Dropping further down within the same list: the raw target position was counted against the
  // list still containing target at _old_index, so once target is lifted out everything after it
  // shifts back by one.
  if (_old_parent_id == _new_parent_id && _old_index < index) {
    index -= 1u;
  }

  auto parent_or_root = _new_parent_id ? target.find(*_new_parent_id) : target.root();

  if (!_new_parent_id || parent_or_root.is_valid()) {
    target.insert_child(parent_or_root, node, index);
  }
}

auto reparent_node_command::undo(sbx::scenes::scene& target) -> void {
  auto node = target.find(_id);

  if (!node.is_valid()) {
    return;
  }

  auto parent_or_root = _old_parent_id ? target.find(*_old_parent_id) : target.root();

  if (!_old_parent_id || parent_or_root.is_valid()) {
    target.insert_child(parent_or_root, node, _old_index);
  }
}

set_active_camera_command::set_active_camera_command(sbx::scenes::scene& scene, const sbx::scenes::node& target)
: _id{target.id()} {
  if (auto current = scene.active_camera(); current.is_valid()) {
    _previous_id = current.id();
  }
}

auto set_active_camera_command::execute(sbx::scenes::scene& target) -> void {
  if (auto node = target.find(_id); node.is_valid()) {
    target.set_active_camera(node);
  }
}

auto set_active_camera_command::undo(sbx::scenes::scene& target) -> void {
  if (_previous_id) {
    if (auto node = target.find(*_previous_id); node.is_valid()) {
      target.set_active_camera(node);
    }
  } else {
    target.set_active_camera(sbx::scenes::node{});
  }
}

} // namespace editor
