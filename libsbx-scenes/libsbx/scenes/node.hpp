// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_NODE_HPP_
#define LIBSBX_SCENES_NODE_HPP_

#include <unordered_set>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

// #include <libsbx/signals/signal.hpp>

// #include <libsbx/memory/observer_ptr.hpp>

namespace sbx::scenes {

// class node {

//   friend class scene;

// public:

//   static const node null;

//   ~node() = default;

//   template<typename Component, typename... Args>
//   auto add_component(Args&&... args) -> Component& {
//     return _registry->emplace<Component>(_entity, std::forward<Args>(args)...);
//   }

//   template<typename Component, typename... Args>
//   auto get_or_add_component(Args&&... args) -> Component& {
//     return _registry->get_or_emplace<Component>(_entity, std::forward<Args>(args)...);
//   }

//   template<typename Component>
//   auto get_component() -> Component& {
//     return _registry->get<Component>(_entity);
//   }

//   template<typename Component>
//   auto get_component() const -> const Component& {
//     return _registry->get<Component>(_entity);
//   }

//   template<typename Component>
//   auto has_component() const -> bool {
//     return _registry->try_get<Component>(_entity) != nullptr;
//   }

//   template<typename Component>
//   auto remove_component() -> void {
//     _registry->remove<Component>(_entity);
//   }

//   auto is_valid() const -> bool {
//     return _registry->is_valid(_entity);
//   }

//   operator bool() const noexcept {
//     return *this != null;
//   }

//   friend auto operator==(const node& lhs, const node& rhs) -> bool {
//     return lhs._registry == rhs._registry && lhs._entity == rhs._entity;
//   }

// private:

//   node(memory::observer_ptr<ecs::registry> registry, ecs::entity entity)
//   : _registry{registry},
//     _entity{entity} { }

//   memory::observer_ptr<ecs::registry> _registry;
//   ecs::entity _entity;

// }; // class node

// class node {

//   friend struct sbx::ecs::entity_traits<node>;

// public:

//   inline static constexpr auto null = sbx::ecs::null_entity;

//   using entity_type = std::uint32_t;

//   constexpr operator entity_type() const noexcept {
//     return _value;
//   }

//   constexpr operator bool() const noexcept {
//     return _value != null;
//   }

// private:

//   explicit constexpr node(const entity_type value) noexcept 
//   : _value{value} { }

//   entity_type _value;

// }; // struct node

enum class node : std::uint32_t {
  null = ecs::null_entity
}; // enum class node

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_NODE_HPP_
