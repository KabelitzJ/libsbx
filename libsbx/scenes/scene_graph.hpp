// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_GRAPH_HPP_
#define LIBSBX_SCENES_SCENE_GRAPH_HPP_

#include <string>
#include <unordered_map>

#include <easy/profiler.h>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

namespace sbx::scenes {

class scene_graph {

public:

  using registry_type = ecs::basic_registry<scenes::node>;

  template<typename... Exclude>
  inline static constexpr auto query_filter = ecs::exclude<Exclude...>;

  scene_graph()
  : _registry{},
    _root{_registry.create()} {
    const auto& root_id = _registry.emplace<scenes::id>(_root);

    _nodes.insert({root_id, _root});

    _registry.emplace<scenes::relationship>(_root, scenes::node::null);
    _registry.emplace<scenes::transform>(_root);
    _registry.emplace<scenes::tag>(_root, "ROOT");
    _registry.emplace<scenes::global_transform>(_root);
  }

  auto root() const -> scenes::node {
    return _root;
  }

  auto node_count() const -> std::size_t {
    return _nodes.size();
  }

  auto is_valid(const scenes::node node) const -> bool {
    return _registry.is_valid(node);
  }

  auto find_node(const scenes::id& id) -> scenes::node {
    if (auto entry = _nodes.find(id); entry != _nodes.end()) {
      return entry->second;
    }

    return scenes::node::null;
  }

  auto create_node(const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> scenes::node {
    return create_child_node(_root, tag, transform);
  }

  auto create_child_node(const scenes::node parent, const std::string& tag = "Node", const scenes::transform& transform = scenes::transform{}) -> scenes::node {
    auto node = _registry.create();

    const auto& id = _registry.emplace<scenes::id>(node);

    _nodes.insert({id, node});

    _registry.emplace<scenes::relationship>(node, parent);
    _registry.get<scenes::relationship>(parent).add_child(node);

    _registry.emplace<scenes::global_transform>(node);
    _registry.emplace<scenes::transform>(node, transform);
    _registry.emplace<scenes::tag>(node, !tag.empty() ? tag : scenes::tag{"Node"});

    return node;
  }

  auto destroy_node(const scenes::node node) -> void {
    const auto& relationship = _registry.get<scenes::relationship>(node);

    if (relationship.parent() != scenes::node::null) {
      _registry.get<scenes::relationship>(relationship.parent()).remove_child(node);
    }

    auto stack = std::vector<std::pair<scenes::node, bool>>{};
    stack.reserve(32u);

    stack.emplace_back(node, false);

    while (!stack.empty()) {
      auto [current_node, visited] = stack.back();
      stack.pop_back();

      if (current_node == scenes::node::null) {
        continue;
      }

      if (!visited) {
        stack.emplace_back(current_node, true);

        const auto& current_relationship = _registry.get<scenes::relationship>(current_node);

        for (auto child : current_relationship.children()) {
          if (child != node::null) {
            stack.emplace_back(child, false);
          }
        }
      } else {
        const auto& id = _registry.get<scenes::id>(current_node);

        _nodes.erase(id);
        _registry.destroy(current_node);
      }
    }
  }

  auto make_child_of(const scenes::node node, const scenes::node parent) -> void {
    if (node == parent) {
      return;
    }

    auto& node_relationship = _registry.get<scenes::relationship>(node);

    if (node_relationship.parent() == parent) {
      return;
    }

    auto& new_parent_relationship = _registry.get<scenes::relationship>(parent);

    if (node_relationship.parent() != scenes::node::null) {
      auto& old_parent_relationship = _registry.get<scenes::relationship>(node_relationship.parent());

      old_parent_relationship.remove_child(node);
    }

    node_relationship.set_parent(parent);
    new_parent_relationship.add_child(node);
  }

  auto reassign_node_id(const scenes::node node, const math::uuid new_id) -> void {
    _nodes.erase(_registry.get<scenes::id>(node));
    _registry.get<scenes::id>(node) = scenes::id{new_id};
    _nodes.insert({new_id, node});
  }

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  template<typename Type, typename... Other, typename... Exclude>
  auto query(ecs::exclude_t<Exclude...> = ecs::exclude_t{}) const -> decltype(auto) {
    return _registry.view<Type, Other...>(ecs::exclude<Exclude...>);
  }

  template<typename Type, typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort(Compare compare, Sort sort = Sort{}, Args&&... args) -> void {
    _registry.sort<Type>(std::move(compare), std::move(sort), std::forward<Args>(args)...);
  }

  template<typename Component>
  auto has_component(const scenes::node node) const -> bool {
    return _registry.all_of<Component>(node);
  }

  template<typename Component, typename... Args>
  auto add_component(const scenes::node node, Args&&... args) -> decltype(auto) {
    return _registry.emplace<Component>(node, std::forward<Args>(args)...);
  }

  template<typename Component, typename... Args>
  auto add_or_update_component(const scenes::node node, Args&&... args) -> decltype(auto) {
    return _registry.emplace_or_replace<Component>(node, std::forward<Args>(args)...);
  }

  template<typename Component>
  auto get_component(const scenes::node node) const -> const Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component>
  auto get_component(const scenes::node node) -> Component& {
    return _registry.get<Component>(node);
  }

  template<typename Component, typename... Args>
  auto get_or_add_component(const scenes::node node, Args&&... args) -> Component& {
    return _registry.get_or_emplace<Component>(node, std::forward<Args>(args)...);
  }

  template<typename Component>
  auto try_get_component(const scenes::node node) -> memory::observer_ptr<Component> {
    return _registry.try_get<Component>(node);
  }

  auto registry() -> registry_type& {
    return _registry;
  }

  auto registry() const -> const registry_type& {
    return _registry;
  }

  auto world_transform(const scenes::node node) -> math::matrix4x4 {
    EASY_BLOCK("scene_graph::world_transform");
    return _ensure_world(node).model;
  }

  auto world_normal(const scenes::node node) -> math::matrix4x4 {
    EASY_BLOCK("scene_graph::world_normal");
    return _ensure_world(node).normal;
  }

  auto parent_transform(const scenes::node node) -> math::matrix4x4 {
    EASY_BLOCK("scene_graph::parent_transform");

    const auto& relationship = _registry.get<scenes::relationship>(node);

    if (relationship.parent() != scenes::node::null) {
      return world_transform(relationship.parent());
    }

    return math::matrix4x4::identity;
  }

  auto world_position(const scenes::node node) -> math::vector3 {
    EASY_BLOCK("scene_graph::world_position");
    return math::vector3{world_transform(node)[3]};
  }

  auto world_rotation(const scenes::node node) -> math::quaternion {
    EASY_BLOCK("scene_graph::world_rotation");
    return math::quaternion{math::matrix4x4::rotation_basis(world_transform(node))};
  }

  auto world_scale(const scenes::node node) -> math::vector3 {
    EASY_BLOCK("scene_graph::world_scale");

    const auto world = world_transform(node);

    return math::vector3{
      math::vector3{world[0]}.length(),
      math::vector3{world[1]}.length(),
      math::vector3{world[2]}.length()
    };
  }

private:

  auto _ensure_world(const scenes::node node) -> const scenes::global_transform& {
    auto chain = utility::make_array<scenes::node, 32u>(scenes::node::null);
    auto chain_size = std::uint32_t{0};

    for (auto current = node; current != scenes::node::null;) {
      chain[chain_size++] = current;

      const auto& relationship = _registry.get<scenes::relationship>(current);

      current = relationship.parent();
    }

    auto parent_world = math::matrix4x4::identity;
    auto parent_world_version = std::uint64_t{0};

    for (auto i = chain_size - 1u; (i >= 0u && i < chain_size); --i) {
      const auto current = chain[i];

      auto& local = _registry.get<scenes::transform>(current);
      auto& world = _registry.get<scenes::global_transform>(current);

      if (world.local_seen != local.version() || world.parent_seen != parent_world_version) {
        world.model = parent_world * local.local_transform();
        world.normal = math::matrix4x4::transposed(math::matrix4x4::inverted(world.model));
        world.local_seen = local.version();
        world.parent_seen = parent_world_version;

        ++world.version;
      }

      parent_world = world.model;
      parent_world_version = world.version;
    }

    return _registry.get<scenes::global_transform>(node);
  }

  registry_type _registry;
  scenes::node _root;
  std::unordered_map<math::uuid, scenes::node> _nodes;

}; // class scene_graph

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_GRAPH_HPP_
