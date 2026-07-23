// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene.hpp>

#include <libsbx/utility/profiler.hpp>

namespace sbx::scenes {

auto scene::create_node() -> node {
  const auto entity = _registry.create();

  _registry.emplace<local_transform>(entity);
  _registry.emplace<world_transform>(entity);
  _registry.emplace<relationship>(entity);

  return node{this, entity};
}

auto scene::set_active_camera(node camera) -> void {
  _active_camera = camera._entity;
}

auto scene::active_camera() -> node {
  return node{this, _active_camera};
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

auto scene::_set_parent(ecs::entity child, ecs::entity parent) -> void {
  _registry.get<relationship>(child).parent = parent;
  _registry.get<relationship>(parent).children.push_back(child);
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
