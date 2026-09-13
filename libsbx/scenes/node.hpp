#ifndef LIBSBX_SCENES_NODE_HPP_
#define LIBSBX_SCENES_NODE_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/ecs/entity.hpp>
#include <libsbx/ecs/registry.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::scenes {

/**
 * @brief A lightweight handle to one node in a scene.
 */
class node {

  friend class scene;
  friend class scene_serializer;

  friend auto operator==(const node& lhs, const node& rhs) noexcept -> bool;

public:

  node() = default;

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _registry != nullptr && _entity != ecs::null_entity && _registry->is_valid(_entity);
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

  [[nodiscard]] auto layer() -> scenes::layer& {
    return get_component<scenes::layer>();
  }

  [[nodiscard]] auto layer() const -> const scenes::layer& {
    return get_component<scenes::layer>();
  }

  // add_component/get_or_add_component below return decltype(auto), not Component&, matching the
  // registry's own emplace/get_or_emplace -- an empty (tag) Component like scenes::inactive gets a
  // storage specialization that tracks presence only, with no per-entity value, so these come back
  // void for it instead of a reference. get_component keeps an explicit Component& since nothing
  // calls it with an empty type yet.
  template<typename Component, typename... Args>
  auto add_component(Args&&... args) -> decltype(auto) {
    return _registry->emplace<Component>(_entity, std::forward<Args>(args)...);
  }

  template<typename Component>
  [[nodiscard]] auto get_component() -> Component& {
    return _registry->get<Component>(_entity);
  }

  template<typename Component>
  [[nodiscard]] auto get_component() const -> const Component& {
    return _registry->get<Component>(_entity);
  }

  template<typename Component, typename... Args>
  [[nodiscard]] auto get_or_add_component(Args&&... args) -> decltype(auto) {
    return _registry->get_or_emplace<Component>(_entity, std::forward<Args>(args)...);
  }


  template<typename Component>
  [[nodiscard]] auto has_component() const -> bool {
    return _registry->all_of<Component>(_entity);
  }

  template<typename Component>
  [[nodiscard]] auto try_get_component() -> memory::observer_ptr<Component> {
    return _registry->try_get<Component>(_entity);
  }

  template<typename Component>
  [[nodiscard]] auto try_get_component() const -> memory::observer_ptr<const Component> {
    return _registry->try_get<Component>(_entity);
  }

  template<typename Component>
  auto remove_component() -> bool {
    return _registry->remove<Component>(_entity) == 1u;
  }

  /** @brief Active by default; false adds an @ref inactive tag, true removes it. See that component's own doc comment for which systems respect it. */
  auto set_active(bool active) -> void {
    if (active) {
      static_cast<void>(remove_component<scenes::inactive>());
    } else {
      static_cast<void>(get_or_add_component<scenes::inactive>());
    }
  }

  [[nodiscard]] auto is_active() const -> bool {
    return !has_component<scenes::inactive>();
  }

  [[nodiscard]] auto transform() -> local_transform& {
    return get_component<local_transform>();
  }

  [[nodiscard]] auto world_matrix() -> const math::matrix4x4& {
    return get_component<world_transform>().matrix;
  }

  /**
   * @brief Reparents this node, first detaching it from whatever parent it already has (if any) so
   * it never ends up listed as a child of two nodes at once. No-op if @p parent is this node or one
   * of its own descendants -- accepting that would create a cycle in relationship::parent/children,
   * and every recursive hierarchy walk (scene update/destroy, serialization) would recurse forever.
   */
  auto set_parent(node& parent) -> void {
    for (auto ancestor = parent._entity; ancestor != ecs::null_entity; ancestor = _registry->get<scenes::relationship>(ancestor).parent) {
      if (ancestor == _entity) {
        return;
      }
    }

    auto& relationship = get_component<scenes::relationship>();

    if (relationship.parent != ecs::null_entity) {
      auto& siblings = _registry->get<scenes::relationship>(relationship.parent).children;
      std::erase(siblings, _entity);
    }

    relationship.parent = parent._entity;
    parent.get_component<scenes::relationship>().children.push_back(_entity);
  }

private:

  node(memory::observer_ptr<ecs::registry> registry, ecs::entity entity)
  : _registry{registry}, _entity{entity} { }

  memory::observer_ptr<ecs::registry> _registry{nullptr};
  ecs::entity _entity{ecs::null_entity};

}; // class node

[[nodiscard]] inline auto operator==(const node& lhs, const node& rhs) noexcept -> bool {
  return lhs._registry == rhs._registry && lhs._entity == rhs._entity;
}

} // namespace sbx::scenes

template<>
struct std::hash<sbx::scenes::node> {
  
  auto operator()(const sbx::scenes::node& node) const noexcept -> std::size_t {
    return node.id().value();
  }

}; // struct std::hash

#endif // LIBSBX_SCENES_NODE_HPP_
