// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene.hpp>

#include <algorithm>

#include <libsbx/utility/profiler.hpp>

namespace sbx::scenes {

scene::scene(const std::string& name)
: _name{name} {
  _root = _registry.create();
  _registry.emplace<relationship>(_root);
}

auto scene::create_node(const utility::hashed_string& name, const scenes::local_transform& transform) -> node {
  return _create_node(name, transform, math::uuid::create());
}

auto scene::create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node {
  return _create_node(name, transform, id);
}

auto scene::find(math::uuid id) -> node {
  if (const auto entry = _entities_by_id.find(id); entry != _entities_by_id.end()) {
    return node{memory::make_observer(_registry), entry->second};
  }

  return node{};
}

auto scene::find(const utility::hashed_string& name) -> node {
  // A scan rather than an index: names are edited in place through node::name(), so an index would go stale on rename.
  for (auto&& [entity, node_tag] : _registry.view<tag>().each()) {
    if (node_tag == name) {
      return node{memory::make_observer(_registry), entity};
    }
  }

  return node{};
}

auto scene::node_of(ecs::entity entity) -> node {
  return node{memory::make_observer(_registry), entity};
}

auto scene::root() -> node {
  return node{memory::make_observer(_registry), _root};
}

auto scene::insert_child(node parent, node child, std::size_t index) -> void {
  // No-op if parent is child itself or one of child's own descendants -- see node::set_parent's
  // identical guard for why (a cycle here recurses forever in every hierarchy walk).
  for (auto ancestor = parent._entity; ancestor != ecs::null_entity; ancestor = _registry.get<relationship>(ancestor).parent) {
    if (ancestor == child._entity) {
      return;
    }
  }

  // A node moved to the top level keeps the scene it's in now; a nested node's own tag may be stale, so it's rewritten.
  if (parent._entity == _root) {
    _registry.emplace_or_replace<scene_membership>(child._entity, scene_of(child));
  }

  auto& child_relationship = _registry.get<relationship>(child._entity);

  if (child_relationship.parent != ecs::null_entity) {
    auto& old_siblings = _registry.get<relationship>(child_relationship.parent).children;
    std::erase(old_siblings, child._entity);
  }

  child_relationship.parent = parent._entity;

  auto& siblings = _registry.get<relationship>(parent._entity).children;
  siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(std::min(index, siblings.size())), child._entity);
}

auto scene::destroy_node(node target) -> void {
  if (!target.is_valid() || target._entity == _root) {
    return;
  }

  const auto entity = target._entity;

  if (const auto& target_relationship = _registry.get<relationship>(entity); target_relationship.parent != ecs::null_entity && _registry.is_valid(target_relationship.parent)) {
    auto& parent_relationship = _registry.get<relationship>(target_relationship.parent);
    std::erase(parent_relationship.children, entity);
  }

  _destroy_node_recursive(entity);
}

auto scene::scene_of(node target) -> std::uint32_t {
  auto entity = target._entity;

  while (entity != ecs::null_entity && _registry.get<relationship>(entity).parent != _root) {
    entity = _registry.get<relationship>(entity).parent;
  }

  if (entity == ecs::null_entity) {
    return _active_membership;
  }

  const auto membership = _registry.try_get<scene_membership>(entity);

  return membership ? membership->handle : _active_membership;
}

auto scene::members_of(std::uint32_t handle) -> std::vector<node> {
  auto members = std::vector<node>{};

  const auto collect = [&](this const auto& self, ecs::entity entity) -> void {
    members.push_back(node{memory::make_observer(_registry), entity});

    for (const auto child : _registry.get<relationship>(entity).children) {
      self(child);
    }
  };

  for (const auto entity : _registry.get<relationship>(_root).children) {
    if (scene_of(node{memory::make_observer(_registry), entity}) == handle) {
      collect(entity);
    }
  }

  return members;
}

auto scene::destroy_scene_nodes(std::uint32_t handle) -> void {
  // Copied: destroy_node erases from the root's children.
  const auto top_level = _registry.get<relationship>(_root).children;

  for (const auto entity : top_level) {
    if (const auto target = node{memory::make_observer(_registry), entity}; scene_of(target) == handle) {
      destroy_node(target);
    }
  }
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

  for (const auto child : _registry.get<relationship>(_root).children) {
    _update_node(child, math::matrix4x4::identity);
  }
}

auto scene::_create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node {
  const auto entity = _registry.create();

  _registry.emplace<local_transform>(entity, transform);
  _registry.emplace<world_transform>(entity);
  _registry.emplace<relationship>(entity);
  _registry.emplace<scenes::id>(entity, id);
  _registry.emplace<scenes::tag>(entity, name);
  _registry.emplace<scenes::layer>(entity);
  _registry.emplace<scene_membership>(entity, _active_membership);

  _entities_by_id.emplace(id, entity);

  _set_parent(entity, _root);

  return node{memory::make_observer(_registry), entity};
}

auto scene::_set_parent(ecs::entity child, ecs::entity parent) -> void {
  _registry.get<relationship>(child).parent = parent;
  _registry.get<relationship>(parent).children.push_back(child);
}

auto scene::_destroy_node_recursive(ecs::entity entity) -> void {
  // Copy out first — destroying a child erases it from this same vector, so iterating it directly would invalidate the iteration.
  const auto children = _registry.get<relationship>(entity).children;

  for (const auto child : children) {
    _destroy_node_recursive(child);
  }

  _entities_by_id.erase(_registry.get<id>(entity));

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
