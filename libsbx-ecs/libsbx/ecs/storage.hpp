// SPDX-License-Identifier: MIT
#ifndef LIBSBX_STORAGE_HPP_
#define LIBSBX_STORAGE_HPP_

#include <type_traits>
#include <iostream>
#include <limits>
#include <vector>
#include <memory>
#include <functional>

#include <libsbx/utility/type_name.hpp>

#include <libsbx/memory/concepts.hpp>
#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/iterable_adaptor.hpp>

#include <libsbx/ecs/sparse_set.hpp>
#include <libsbx/ecs/component.hpp>
#include <libsbx/ecs/meta.hpp>

#include <libsbx/ecs/detail/storage_iterator.hpp>

namespace sbx::ecs {

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator = std::allocator<Type>>
class basic_storage : public basic_sparse_set<Entity, memory::rebound_allocator_t<Allocator, Entity>> {

  using allocator_traits = std::allocator_traits<Allocator>;
  using container_type = std::vector<typename allocator_traits::pointer, memory::rebound_allocator_t<Allocator, typename allocator_traits::pointer>>;
  using underlying_type = basic_sparse_set<Entity, memory::rebound_allocator_t<Allocator, Entity>>;
  using underlying_iterator = typename underlying_type::basic_iterator;
  using component_traits = ecs::component_traits<Type, Entity>;

public:

  using allocator_type = Allocator;
  using base_type = underlying_type;
  using element_type = Type;
  using value_type = element_type;
  using entity_type = Entity;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = typename container_type::pointer;
  using const_pointer = typename allocator_traits::template rebind_traits<typename allocator_traits::const_pointer>::const_pointer;
  using iterator = detail::storage_iterator<container_type, component_traits::page_size>;
  using const_iterator = detail::storage_iterator<const container_type, component_traits::page_size>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::iterator, iterator>>;
  using const_iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::const_iterator, const_iterator>>;

  static constexpr auto storage_policy = static_cast<deletion_policy>(component_traits::in_place_delete);

  basic_storage();

  explicit basic_storage(const allocator_type &allocator);

  basic_storage(const basic_storage& other) = delete;

  basic_storage(basic_storage&& other) noexcept;

  ~basic_storage() override;

  auto operator=(const basic_storage& other) -> basic_storage& = delete;

  auto operator=(basic_storage&& other) noexcept -> basic_storage&;

  auto swap(basic_storage& other) noexcept -> void;

  constexpr auto get_allocator() const noexcept -> allocator_type;

  auto reserve(const size_type capacity) -> void override;

  [[nodiscard]] auto get(const entity_type entity) const noexcept -> const value_type&;

  [[nodiscard]] auto get(const entity_type entity) noexcept -> value_type&;

  [[nodiscard]] auto get_as_tuple(const entity_type entity) const noexcept -> std::tuple<const value_type&>;

  [[nodiscard]] auto get_as_tuple(const entity_type entity) noexcept -> std::tuple<value_type&>;

  [[nodiscard]] auto cbegin() const noexcept -> const_iterator;

  [[nodiscard]] auto begin() const noexcept -> const_iterator;

  [[nodiscard]] auto begin() noexcept -> iterator;

  [[nodiscard]] auto cend() const noexcept -> const_iterator;

  [[nodiscard]] auto end() const noexcept -> const_iterator;

  [[nodiscard]] auto end() noexcept -> iterator;

  template<typename... Args>
  requires (std::is_constructible_v<value_type, Args...>)
  auto emplace(const entity_type entity, Args&&... args) -> value_type&;

  template<typename Function>
  requires (std::is_invocable_v<Function, value_type&>)
  auto patch(const entity_type entity, Function&& function) -> value_type&;

  template<typename Iterator>
  auto insert(iterator first, iterator last, const value_type& value = value_type{}) -> iterator;

  [[nodiscard]] auto each() noexcept -> iterable;

  [[nodiscard]] auto each() const noexcept -> const_iterable;

  template<typename Callable>
  requires (std::is_invocable_r_v<void, Callable, const entity_type, Type&>)
  auto add_meta(const utility::hashed_string& tag, Callable&& callable) -> void;

protected:

  auto call(const utility::hashed_string& tag, const entity_type entity) -> void override;

  auto pop(underlying_iterator first, underlying_iterator last) -> void override;

  auto pop_all() -> void override;

  auto try_emplace([[maybe_unused]] const entity_type entity, [[maybe_unused]] const bool force_back) -> underlying_iterator override;

private:

  auto _swap_or_move([[maybe_unused]] const std::size_t from, [[maybe_unused]] const std::size_t to) -> void override;

  auto _element_at(const std::size_t position) const -> const value_type&;

  auto _element_at(const std::size_t position) -> value_type&;

  auto _assure_at_least(const std::size_t position);

  template<typename... Args>
  requires (std::is_constructible_v<value_type, Args...>)
  auto _emplace_element(const entity_type entity, const bool force_back, Args&&... args);

  auto _shrink_to_size(const size_type size) -> void;

  void _swap_at(const size_type lhs, const size_type rhs);

  void _move_to(const size_type lhs, const size_type rhs);

  container_type _container;

}; // class basic_storage

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
class basic_storage<Type, Entity, Allocator> : public basic_sparse_set<Entity, memory::rebound_allocator_t<Allocator, Entity>> {

  using allocator_traits = std::allocator_traits<Allocator>;
  using component_traits = ecs::component_traits<Type, Entity>;

public:

  using allocator_type = Allocator;
  using base_type = basic_sparse_set<Entity, memory::rebound_allocator_t<Allocator, Entity>>;
  using element_type = Type;
  using value_type = void;
  using entity_type = Entity;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::iterator>>;
  using const_iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::const_iterator>>;

  using reverse_iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::reverse_iterator>>;
  using const_reverse_iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::const_reverse_iterator>>;

  static constexpr auto storage_policy = static_cast<deletion_policy>(component_traits::in_place_delete);

  basic_storage();

  explicit basic_storage(const allocator_type& allocator);

  basic_storage(const basic_storage& other) = delete;

  basic_storage(basic_storage&& other) noexcept = default;

  basic_storage(basic_storage&& other, const allocator_type& allocator);

  ~basic_storage() override = default;

  auto operator=(const basic_storage& other) -> basic_storage& = delete;

  auto operator=(basic_storage&& other) noexcept -> basic_storage& = default;

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type;

  auto get([[maybe_unused]] const entity_type entity) const noexcept -> void;

  [[nodiscard]] auto get_as_tuple([[maybe_unused]] const entity_type entity) const noexcept -> std::tuple<>;

  template<typename... Args>
  auto emplace(const entity_type entity, Args&& ...) -> void;

  template<typename Function>
  requires (std::is_invocable_v<Function>)
  auto patch([[maybe_unused]] const entity_type entity, Function&& function) -> void;

  [[nodiscard]] auto each() noexcept -> iterable;

  [[nodiscard]] auto each() const noexcept -> const_iterable;

}; // class basic_storage

template<typename Entity, memory::allocator_for<Entity> Allocator>   
class basic_storage<Entity, Entity, Allocator> : public basic_sparse_set<Entity, Allocator> { 

  using allocator_traits = std::allocator_traits<Allocator>;
  using underlying_iterator = typename basic_sparse_set<Entity, Allocator>::basic_iterator;
  using entity_traits = ecs::entity_traits<Entity>;

public:

  using allocator_type = Allocator;
  using base_type = basic_sparse_set<Entity, Allocator>;
  using value_type = void;
  using entity_type = Entity;
  using element_type = Entity;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::iterator>>;
  using const_iterable = memory::iterable_adaptor<detail::extended_storage_iterator<typename base_type::const_iterator>>;

  static constexpr auto storage_policy = deletion_policy::swap_only;

  basic_storage();

  explicit basic_storage(const allocator_type& allocator);

  basic_storage(const basic_storage& other) = delete;

  basic_storage(basic_storage&& other) noexcept;

  ~basic_storage() override = default;

  auto operator=(const basic_storage& other) -> basic_storage& = delete;

  auto operator=(basic_storage&& other) noexcept -> basic_storage&;

  auto get([[maybe_unused]] const entity_type entity) const noexcept -> void;

  [[nodiscard]] auto get_as_tuple([[maybe_unused]] const entity_type entity) const noexcept -> std::tuple<>;

  auto generate() -> entity_type;

  auto generate(const entity_type hint) -> entity_type;

  template<typename Function>
  requires (std::is_invocable_v<Function>)
  auto patch([[maybe_unused]] const entity_type entity, Function&& function) -> void;

  [[nodiscard]] auto each() noexcept -> iterable;

  [[nodiscard]] auto each() const noexcept -> const_iterable;

protected:

  auto pop_all() -> void override;

  auto try_emplace(const entity_type hint, const bool) -> underlying_iterator override;

private:

  auto _from_placeholder() noexcept;

  auto _next() noexcept;

  size_type _placeholder;

}; // class basic_storage

} // namespace sbx::ecs

#include <libsbx/ecs/storage.ipp>

#endif // LIBSBX_STORAGE_HPP_
