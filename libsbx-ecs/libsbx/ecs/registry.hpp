// SPDX-License-Identifier: MIT
#ifndef LIBSBX_REGISTRY_HPP_
#define LIBSBX_REGISTRY_HPP_

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <optional>
#include <algorithm>
#include <tuple>
#include <ranges>

#include <libsbx/utility/type_id.hpp>
#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/algorithm.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/assert.hpp>

#include <libsbx/memory/concepts.hpp>
#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/containers/dense_map.hpp>

#include <libsbx/ecs/entity.hpp>
#include <libsbx/ecs/sparse_set.hpp>
#include <libsbx/ecs/storage.hpp>
#include <libsbx/ecs/view.hpp>

#include <libsbx/ecs/detail/registry_storage_iterator.hpp>

namespace sbx::ecs {

namespace detail {

struct ecs_type_id_scope { };

} // namespace detail

/**
 * @brief A scoped type ID generator for the libsbx-ecs scope.
 *
 * @tparam Type The type for which the ID is generated.
 */
template<typename Type>
using type_id = utility::scoped_type_id<detail::ecs_type_id_scope, Type>;

template<typename Type, typename Entity = entity, memory::allocator_for<Type> Allocator = std::allocator<Type>>
struct storage_type {
  using type = basic_storage<Type, Entity, Allocator>;
}; // struct storage_type

template<typename... Args>
using storage_type_t = typename storage_type<Args...>::type;

template<typename Type, typename Entity = entity, memory::allocator_for<std::remove_const_t<Type>> Allocator = std::allocator<std::remove_const_t<Type>>>
struct storage_for {
  using type = utility::constness_as_t<storage_type_t<std::remove_const_t<Type>, Entity, Allocator>, Type>;
}; // struct storage_for

template<typename... Args>
using storage_for_t = typename storage_for<Args...>::type;

template<typename Entity, memory::allocator_for<Entity> Allocator = std::allocator<Entity>>
class basic_registry {

  using base_type = basic_sparse_set<Entity, Allocator>;
  using allocator_traits = std::allocator_traits<Allocator>;

  using pool_container_type = containers::dense_map<std::uint32_t, std::shared_ptr<base_type>, std::identity, std::equal_to<>, memory::rebound_allocator_t<Allocator, std::pair<const std::uint32_t, std::shared_ptr<base_type>>>>;
  using entity_traits = ecs::entity_traits<Entity>;

  template<typename Type>
  using storage_for_type = storage_for_t<Type, Entity, memory::rebound_allocator_t<Allocator, std::remove_const_t<Type>>>;

public:

  using allocator_type = Allocator;
  using entity_type = entity_traits::value_type;
  using version_type = entity_traits::version_type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using common_type = base_type;
  using iterable = memory::iterable_adaptor<detail::registry_storage_iterator<typename pool_container_type::iterator>>;
  using const_iterable = memory::iterable_adaptor<detail::registry_storage_iterator<typename pool_container_type::const_iterator>>;

  // template<typename... Get, typename... Exclude>
  // using view_type = basic_view<get_t<storage_for_type<Get>...>, exclude_t<storage_for_type<Exclude>...>>;

  // template<typename Type, typename... Other, typename... Exclude>
  // using const_view_type = view_type<const Type, const Other..., const Exclude...>;

  basic_registry()
  : basic_registry{allocator_type{}} { }

  explicit basic_registry(const allocator_type& allocator)
  : basic_registry{0u, allocator} { }

  basic_registry(const size_type count, const allocator_type &allocator = allocator_type{})
  : _pools{allocator},
    _entities{allocator} {
    _pools.reserve(count);
  }

  basic_registry(const basic_registry& other) = delete;

  basic_registry(basic_registry&& other) noexcept
  : _pools{std::move(other._pools)},
    _entities{std::move(other._entities)} { }

  ~basic_registry() = default;

  auto operator=(const basic_registry& other) -> basic_registry& = delete;

  auto operator=(basic_registry&& other) noexcept -> basic_registry& {
    swap(other);
    return *this;
  }

  auto swap(basic_registry& other) noexcept -> void {
    using std::swap;
    swap(_pools, other._pools);
    swap(_entities, other._entities);
  }

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type {
    return _entities.get_allocator();
  }

  [[nodiscard]] auto is_valid(const entity_type entity) const -> bool {
    return static_cast<size_type>(_entities.find(entity).index()) < _entities.free_list();
  }

  auto create() -> entity_type {
    return _entities.generate();
  }

  auto destroy(const entity_type entity) -> version_type {
    for (auto position = _pools.size(); position != 0u; --position) {
      _pools.begin()[static_cast<typename pool_container_type::difference_type>(position - 1u)].second->remove(entity);
    }

    _entities.erase(entity);
    return _entities.current(entity);
  }

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  auto emplace(const entity_type entity, Args&&... args) -> decltype(auto) {
    utility::assert_that(is_valid(entity), "Invalid entity");
    return _assure<Type>().emplace(entity, std::forward<Args>(args)...);
  }

  template<typename Type, typename... Other>
  auto remove(const entity_type entity) -> size_type {
    return (_assure<Type>().remove(entity) + ... + _assure<Other>().remove(entity));
  }

  template<typename... Type>
  [[nodiscard]] auto all_of([[maybe_unused]] const entity_type entity) const -> bool {
    if constexpr(sizeof...(Type) == 1u) {
      auto* pool = _assure<std::remove_const_t<Type>...>();
      return pool && pool->contains(entity);
    } else {
      return (all_of<Type>(entity) && ...);
    }
  }

  template<typename... Type>
  [[nodiscard]] auto any_of([[maybe_unused]] const entity_type entity) const -> bool {
    return (all_of<Type>(entity) || ...);
  }

  template<typename... Type>
  [[nodiscard]] auto get([[maybe_unused]] const entity_type entity) const -> decltype(auto) {
    if constexpr (sizeof...(Type) == 1u) {
      return (_assure<std::remove_const_t<Type>>()->get(entity), ...);
    } else {
      return std::forward_as_tuple(get<Type>(entity)...);
    }
  }

  template<typename... Type>
  [[nodiscard]] auto get([[maybe_unused]] const entity_type entity) -> decltype(auto) {
    if constexpr (sizeof...(Type) == 1u) {
      return (static_cast<storage_for_type<Type>&>(_assure<std::remove_const_t<Type>>()).get(entity), ...);
    } else {
      return std::forward_as_tuple(get<Type>(entity)...);
    }
  }

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  [[nodiscard]] auto get_or_emplace(const entity_type entity, Args&&... args) -> decltype(auto) {
    auto& pool = _assure<Type>();
    utility::assert_that(is_valid(entity), "Invalid entity");
    return pool.contains(entity) ? pool.get(entity) : pool.emplace(entity, std::forward<Args>(args)...);
  }

  template<typename... Type>
  [[nodiscard]] auto try_get([[maybe_unused]] const entity_type entity) const -> decltype(auto) {
    if constexpr (sizeof...(Type) == 1u) {
      const auto* pool = _assure<std::remove_const_t<Type>...>();
      return (pool && pool->contains(entity)) ? std::addressof(pool->get(entity)) : nullptr;
    } else {
      return std::make_tuple(try_get<Type>(entity)...);
    }
  }

  template<typename... Type>
  [[nodiscard]] auto try_get([[maybe_unused]] const entity_type entity) -> decltype(auto) {
    if constexpr (sizeof...(Type) == 1u) {
      return (const_cast<Type*>(std::as_const(*this).template try_get<Type>(entity)), ...);
    } else {
      return std::make_tuple(try_get<Type>(entity)...);
    }
  }

  template<typename... Type>
  auto clear() -> void {
    if constexpr (sizeof...(Type) == 0u) {
      for (auto position = _pools.size(); position; --position) {
        _pools.begin()[static_cast<typename pool_container_type::difference_type>(position - 1u)].second->clear();
      }

      const auto element = _entities.each();
      _entities.erase(element.begin().base(), element.end().base());
    } else {
      (_assure<Type>().clear(), ...);
    }
  }

  template<typename Type, typename... Other, typename... Exclude>
  [[nodiscard]] auto view(exclude_t<Exclude...> = exclude_t{}) const -> basic_view<get_t<storage_for_type<const Type>, storage_for_type<const Other>...>, exclude_t<storage_for_type<const Exclude>...>> {
    auto view = basic_view<get_t<storage_for_type<const Type>, storage_for_type<const Other>...>, exclude_t<storage_for_type<const Exclude>...>>{};
    [&view](const auto* ...current) { ((current ? view.set_storage(*current) : void()), ...); }(_assure<std::remove_const_t<Exclude>>()..., _assure<std::remove_const_t<Other>>()..., _assure<std::remove_const_t<Type>>());
    return view;
  }

  template<typename Type, typename... Other, typename... Exclude>
  [[nodiscard]] auto view(exclude_t<Exclude...> = exclude_t{}) -> basic_view<get_t<storage_for_type<Type>, storage_for_type<Other>...>, exclude_t<storage_for_type<Exclude>...>> {
    return basic_view<get_t<storage_for_type<Type>, storage_for_type<Other>...>, exclude_t<storage_for_type<Exclude>...>>{_assure<std::remove_const_t<Type>>(), _assure<std::remove_const_t<Other>>()..., _assure<std::remove_const_t<Exclude>>()...};
  }

  template<typename Type, typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort(Compare compare, Sort sort = Sort{}, Args&&... args) -> void {
    // utility::assert_that(!owned<Type>(), "Cannot sort owned storage");
    auto& pool = _assure<Type>();

    if constexpr(std::is_invocable_v<Compare, decltype(pool.get(std::declval<entity_type>())), decltype(pool.get(std::declval<entity_type>()))>) {
      auto component_compare = [&pool, compare = std::move(compare)](const auto lhs, const auto rhs) { return compare(std::as_const(pool.get(lhs)), std::as_const(pool.get(rhs))); };
      pool.sort(std::move(component_compare), std::move(sort), std::forward<Args>(args)...);
    } else {
      pool.sort(std::move(compare), std::move(sort), std::forward<Args>(args)...);
    }
  }

  [[nodiscard]] auto storage() noexcept -> iterable {
    return iterable{_pools.begin(), _pools.end()};
  }

  [[nodiscard]] auto storage() const noexcept -> const_iterable {
    return const_iterable{_pools.cbegin(), _pools.cend()};
  }

  [[nodiscard]] auto begin() const -> decltype(auto) {
    return _entities.begin();
  }

  [[nodiscard]] auto end() const -> decltype(auto) {
    return _entities.end();
  }

  template<typename Callable>
  auto invoke(const utility::hashed_string& tag, Callable&& callable) -> void {
    for (const auto entity : _entities | ranges::views::filter(std::forward<Callable>(callable))) {
      for (auto&& [type, storage] : storage()) {
        storage.invoke(tag, entity);
      }
    }
  }

private:

  template<typename Type>
  requires (std::is_same_v<Type, std::decay_t<Type>>)
  [[nodiscard]] auto _assure([[maybe_unused]] const std::uint32_t id = type_id<Type>::value()) -> storage_for_type<Type>& {
    if constexpr(std::is_same_v<Type, entity_type>) {
      utility::assert_that(id == type_id<Type>::value(), "User entity storage not allowed");
      return _entities;
    } else {
      using storage_type = storage_for_type<Type>;

      if (auto iterator = _pools.find(id); iterator != _pools.cend()) {
        return static_cast<storage_type&>(*iterator->second);
      }

      using storage_allocator_type = typename storage_type::allocator_type;
      using pool_type = typename pool_container_type::mapped_type;

      auto pool = pool_type{};

      if constexpr (std::is_void_v<Type> && !std::is_constructible_v<storage_allocator_type, allocator_type>) {
        pool = std::allocate_shared<storage_type>(get_allocator(), storage_allocator_type{});
      } else {
        pool = std::allocate_shared<storage_type>(get_allocator(), get_allocator());
      }

      _pools.emplace(id, pool);

      return static_cast<storage_type&>(*pool);
    }
  }

  template<typename Type>
  requires (std::is_same_v<Type, std::decay_t<Type>>)
  [[nodiscard]] auto _assure([[maybe_unused]] const std::uint32_t id = type_id<Type>::value()) const -> const storage_for_type<Type>* {
    if constexpr(std::is_same_v<Type, entity_type>) {
      utility::assert_that(id == type_id<Type>::value(), "User entity storage not allowed");
      return &_entities;
    } else {
      if (const auto iterator = _pools.find(id); iterator != _pools.cend()) {
        return static_cast<const storage_for_type<Type>*>(iterator->second.get());
      }

      return static_cast<const storage_for_type<Type>*>(nullptr);
    }
  }

  pool_container_type _pools;
  storage_for_type<entity_type> _entities;

}; // class basic_registry

using registry = basic_registry<entity, memory::allocator_type<entity>>;

} // namespace sbx::ecs

#endif // LIBSBX_REGISTRY_HPP_
