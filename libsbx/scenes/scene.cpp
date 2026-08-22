// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene.hpp>

#include <libsbx/utility/profiler.hpp>

namespace sbx::scenes {

auto scene::create_node(const utility::hashed_string& name, const scenes::local_transform& transform) -> node {
  return _create_node(name, transform, math::uuid::create());
}

auto scene::find(math::uuid id) -> node {
  if (const auto entry = _entities_by_id.find(id); entry != _entities_by_id.end()) {
    return node{memory::make_observer(_registry), entry->second};
  }

  return node{};
}

auto scene::find(const utility::hashed_string& name) -> node {
  if (const auto [first, last] = _entities_by_name.equal_range(name); first != last) {
    return node{memory::make_observer(_registry), first->second};
  }

  return node{};
}

auto scene::node_of(ecs::entity entity) -> node {
  return node{memory::make_observer(_registry), entity};
}

auto scene::destroy_node(node target) -> void {
  if (!target.is_valid()) {
    return;
  }

  const auto entity = target._entity;

  if (const auto& target_relationship = _registry.get<relationship>(entity); target_relationship.parent != ecs::null_entity && _registry.is_valid(target_relationship.parent)) {
    auto& parent_relationship = _registry.get<relationship>(target_relationship.parent);
    std::erase(parent_relationship.children, entity);
  }

  _destroy_node_recursive(entity);
}

auto scene::set_active_camera(node camera) -> void {
  _active_camera = camera._entity;
}

auto scene::active_camera() -> node {
  return node{memory::make_observer(_registry), _active_camera};
}

auto scene::set_primary_light(node light) -> void { 
  _primary_light = light._entity;
}

auto scene::primary_light() -> node { 
  return node{memory::make_observer(_registry), _primary_light}; 
}

auto scene::update() -> void {
  SBX_PROFILE_SCOPE("scene::update");

  auto view = _registry.view<local_transform, relationship, world_transform>();

  for (const auto entity : view) {
    if (_registry.get<relationship>(entity).parent == ecs::null_entity) {
      _update_node(entity, math::matrix4x4::identity);
    }
  }
}

auto scene::_create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node {
  const auto entity = _registry.create();

  _registry.emplace<local_transform>(entity, transform);
  _registry.emplace<world_transform>(entity);
  _registry.emplace<relationship>(entity);
  _registry.emplace<scenes::id>(entity, id);
  _registry.emplace<scenes::tag>(entity, name);

  _entities_by_id.emplace(id, entity);
  _entities_by_name.emplace(name, entity);

  return node{memory::make_observer(_registry), entity};
}

auto scene::_set_parent(ecs::entity child, ecs::entity parent) -> void {
  _registry.get<relationship>(child).parent = parent;
  _registry.get<relationship>(parent).children.push_back(child);
}

auto scene::_destroy_node_recursive(ecs::entity entity) -> void {
  // Copy out first — children are destroyed (and thus erased from this very vector, one level up)
  // while we're still iterating it otherwise.
  const auto children = _registry.get<relationship>(entity).children;

  for (const auto child : children) {
    _destroy_node_recursive(child);
  }

  const auto& node_id = _registry.get<id>(entity);
  const auto& node_tag = _registry.get<tag>(entity);

  _entities_by_id.erase(node_id);

  if (const auto [first, last] = _entities_by_name.equal_range(node_tag); first != last) {
    for (auto entry = first; entry != last; ++entry) {
      if (entry->second == entity) {
        _entities_by_name.erase(entry);
        break;
      }
    }
  }

  if (_active_camera == entity) {
    _active_camera = ecs::null_entity;
  }

  if (_primary_light == entity) {
    _primary_light = ecs::null_entity;
  }

  _registry.destroy(entity);
}

auto scene::_update_node(ecs::entity entity, const math::matrix4x4& parent_world) -> void {
  const auto& local = _registry.get<local_transform>(entity);
  auto& world = _registry.get<world_transform>(entity);

  world.matrix = parent_world * local.matrix();

  for (const auto child : _registry.get<relationship>(entity).children) {
    _update_node(child, world.matrix);
  }
}

} // namespace sbx::scenes^
