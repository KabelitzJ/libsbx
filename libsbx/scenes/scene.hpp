// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENE_HPP_
#define LIBSBX_SCENES_SCENE_HPP_

#include <utility>
#include <unordered_map>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::scenes {

/**
 * @brief A scene owns the ECS registry and a transform hierarchy.
 */
class scene {

  friend class scene_serializer;

public:

  /**
   * @brief A lightweight handle to one node in a scene.
   */
  class node {

    friend class scene;

  public:

    node() = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool {
      return _scene != nullptr && _entity != ecs::null_entity;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return is_valid();
    }

    [[nodiscard]] auto id() const -> const scenes::id& {
      return get_component<scenes::id>();
    }

    [[nodiscard]] auto name() -> tag& {
      return get_component<scenes::tag>();
    }

    [[nodiscard]] auto name() const -> const tag& {
      return get_component<scenes::tag>();
    }

    template<typename Component, typename... Args>
    auto add_component(Args&&... args) -> Component& {
      return _scene->_registry.emplace<Component>(_entity, std::forward<Args>(args)...);
    }

    template<typename Component>
    [[nodiscard]] auto get_component() -> Component& {
      return _scene->_registry.get<Component>(_entity);
    }

    template<typename Component>
    [[nodiscard]] auto get_component() const -> const Component& {
      return _scene->_registry.get<Component>(_entity);
    }

    template<typename Component>
    [[nodiscard]] auto has_component() const -> bool {
      return _scene->_registry.all_of<Component>(_entity);
    }

    template<typename Component>
    auto remove_component() -> void {
      _scene->_registry.remove<Component>(_entity);
    }

    [[nodiscard]] auto transform() -> local_transform& {
      return get_component<local_transform>();
    }

    [[nodiscard]] auto world_matrix() -> const math::matrix4x4& {
      return get_component<world_transform>().matrix;
    }

    auto set_parent(node parent) -> void {
      _scene->_set_parent(_entity, parent._entity);
    }

  private:

    node(scene* scene, ecs::entity entity)
    : _scene{scene}, _entity{entity} { }

    memory::observer_ptr<scene> _scene{nullptr};
    ecs::entity _entity{ecs::null_entity};

  }; // class node

  scene() = default;

  auto create_node(const utility::hashed_string& name = "Node", const scenes::local_transform& transform = scenes::local_transform{}) -> node;

  [[nodiscard]] auto find(math::uuid id) -> node;

  auto set_active_camera(node camera) -> void;

  [[nodiscard]] auto active_camera() -> node;

  [[nodiscard]] auto has_active_camera() const noexcept -> bool {
    return _active_camera != ecs::null_entity;
  }

  auto set_primary_light(node light) -> void;

  [[nodiscard]] auto primary_light() -> node;

  [[nodiscard]] auto has_primary_light() const noexcept -> bool {
    return _active_camera != ecs::null_entity;
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

private:

  auto _create_node(const utility::hashed_string& name, const scenes::local_transform& transform, const math::uuid& id) -> node;

  auto _set_parent(ecs::entity child, ecs::entity parent) -> void;

  auto _update_node(ecs::entity entity, const math::matrix4x4& parent_world) -> void;

  ecs::registry _registry{};
  std::unordered_map<math::uuid, ecs::entity> _entities_by_id{};
  ecs::entity _active_camera{ecs::null_entity};
  ecs::entity _primary_light{ecs::null_entity};

}; // class scene

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_HPP_
