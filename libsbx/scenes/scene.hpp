// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENE_HPP_
#define LIBSBX_SCENES_SCENE_HPP_

#include <cstddef>
#include <utility>
#include <unordered_map>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>

namespace sbx::scenes {

/**
 * @brief A scene owns the ECS registry and a transform hierarchy.
 */
class scene {

  friend class scene_serializer;

public:

  scene(const std::string& name = "Scene");

  auto create_node(const utility::hashed_string& name = "Node", const scenes::local_transform& transform = scenes::local_transform{}) -> node;

  /**
   * @brief Creates a node with an explicit id instead of minting a fresh one — for recreating a
   * node undo/redo already knows the id of (e.g. redoing a create, or restoring a deleted node).
   */
  auto create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node;

  [[nodiscard]] auto find(math::uuid id) -> node;

  [[nodiscard]] auto find(const utility::hashed_string& name) -> node;

  /**
   * @brief Wraps an arbitrary entity (e.g. from scene::query<>() or relationship::children) in a
   * node handle. The returned node's validity still depends on the entity being alive — check
   * node::is_valid() before using it.
   */
  [[nodiscard]] auto node_of(ecs::entity entity) -> node;

  /**
   * @brief Sentinel entity whose relationship::children is the ordered list of top-level nodes.
   * Not a real node — never pass it to find(), destroy_node(), or the serializer.
   */
  [[nodiscard]] auto root() -> node;

  /**
   * @brief Detaches child from its current parent and inserts it at index (clamped) among
   * parent's children — pass root() as parent for a top-level position. General-purpose
   * reordering, not just undo/redo: node::set_parent() only ever appends at the end.
   */
  auto insert_child(node parent, node child, std::size_t index) -> void;

  /**
   * @brief Destroys @p target and its entire subtree, unlinking it from its parent's
   * relationship.children first. No-op if target is already invalid.
   */
  auto destroy_node(node target) -> void;

  auto set_active_camera(node camera) -> void;

  [[nodiscard]] auto active_camera() -> node;

  [[nodiscard]] auto has_active_camera() const noexcept -> bool {
    return _active_camera != ecs::null_entity;
  }

  auto set_primary_light(node light) -> void;

  [[nodiscard]] auto primary_light() -> node;

  [[nodiscard]] auto has_primary_light() const noexcept -> bool {
    return _primary_light != ecs::null_entity;
  }

  /**
   * @brief Recomputes every node's world_transform from the hierarchy. Once per frame before extract.
   */
  auto update() -> void;

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) const -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& { 
    return _name; 
  }

  auto set_name(std::string name) -> void { 
    _name = std::move(name);
  }

private:

  auto _create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node;

  auto _set_parent(ecs::entity child, ecs::entity parent) -> void;

  auto _destroy_node_recursive(ecs::entity entity) -> void;

  auto _update_node(ecs::entity entity, const math::matrix4x4& parent_world) -> void;

  std::string _name{"Scene"};
  ecs::registry _registry{};
  std::unordered_map<math::uuid, ecs::entity> _entities_by_id{};
  std::unordered_multimap<utility::hashed_string, ecs::entity> _entities_by_name{};
  ecs::entity _root{ecs::null_entity};
  ecs::entity _active_camera{ecs::null_entity};
  ecs::entity _primary_light{ecs::null_entity};

}; // class scene

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_HPP_
