// SPDX-License-Identifier: MIT
#include <libsbx/ecs/sparse_set.hpp>

namespace sbx::ecs {

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
basic_storage<Type, Entity, Allocator>::basic_storage()
: basic_storage{allocator_type{}} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
basic_storage<Type, Entity, Allocator>::basic_storage(const allocator_type &allocator)
: base_type{storage_policy, allocator},
  _container{allocator} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
basic_storage<Type, Entity, Allocator>::basic_storage(basic_storage&& other) noexcept
: base_type{std::move(other)},
  _container{std::move(other._container)} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
basic_storage<Type, Entity, Allocator>::~basic_storage() {
  _shrink_to_size(0u);
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::operator=(basic_storage&& other) noexcept -> basic_storage& {
  utility::assert_that(allocator_traits::is_always_equal::value || get_allocator() == other.get_allocator(), "Copying a storage is not allowed");
  swap(other);
  return *this;
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::swap(basic_storage& other) noexcept -> void {
  using std::swap;
  swap(_container, other._container);
  base_type::swap(other);
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
constexpr auto basic_storage<Type, Entity, Allocator>::get_allocator() const noexcept -> allocator_type {
  return _container.get_allocator();
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::reserve(const size_type capacity) -> void {
  if (capacity != 0u) {
    base_type::reserve(capacity);
    _assure_at_least(capacity - 1u);
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::get(const entity_type entity) const noexcept -> const value_type& {
  return _element_at(base_type::index(entity));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::get(const entity_type entity) noexcept -> value_type& {
  return const_cast<value_type&>(std::as_const(*this).get(entity));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::get_as_tuple(const entity_type entity) const noexcept -> std::tuple<const value_type&> {
  return std::forward_as_tuple(get(entity));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::get_as_tuple(const entity_type entity) noexcept -> std::tuple<value_type&> {
  return std::forward_as_tuple(get(entity));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::cbegin() const noexcept -> const_iterator {
  const auto position = static_cast<difference_type>(base_type::size());
  return const_iterator{&_container, position};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::begin() const noexcept -> const_iterator {
  return cbegin();
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::begin() noexcept -> iterator {
  const auto position = static_cast<difference_type>(base_type::size());
  return iterator{&_container, position};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::cend() const noexcept -> const_iterator {
  return const_iterator{&_container, {}};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::end() const noexcept -> const_iterator {
  return cend();
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::end() noexcept -> iterator {
  return iterator{&_container, {}};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
template<typename... Args>
requires (std::is_constructible_v<Type, Args...>)
auto basic_storage<Type, Entity, Allocator>::emplace(const entity_type entity, Args&&... args) -> Type& {
  if constexpr (std::is_aggregate_v<value_type> && (sizeof...(Args) != 0u || !std::is_default_constructible_v<value_type>)) {
    const auto it = _emplace_element(entity, false, Type{std::forward<Args>(args)...});
    return _element_at(static_cast<size_type>(it.index()));
  } else {
    const auto it = _emplace_element(entity, false, std::forward<Args>(args)...);
    return _element_at(static_cast<size_type>(it.index()));
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
template<typename Function>
requires (std::is_invocable_v<Function, Type&>)
auto basic_storage<Type, Entity, Allocator>::patch(const entity_type entity, Function&& function) -> Type& {
  const auto index = base_type::index(entity);
  auto& element = _element_at(index);

  std::invoke(std::forward<Function>(function), element);

  return element;
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
template<typename Iterator>
auto basic_storage<Type, Entity, Allocator>::insert(iterator first, iterator last, const Type& value) -> iterator {
  for (; first != last; ++first) {
    _emplace_element(*first, true, value);
  }

  return begin();
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::each() noexcept -> iterable {
  return iterable{{base_type::begin(), begin()}, {base_type::end(), end()}};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::each() const noexcept -> const_iterable {
  return const_iterable{{base_type::cbegin(), cbegin()}, {base_type::cend(), cend()}};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
template<typename Callable>
requires (std::is_invocable_r_v<void, Callable, const Entity, Type&>)
auto basic_storage<Type, Entity, Allocator>::add_meta(const utility::hashed_string& tag, Callable&& callable) -> void {
  auto& functions = base_type::meta();

  functions[tag] = [c = std::forward<Callable>(callable)](const entity_type entity, void* value) {
    std::invoke(c, entity, *static_cast<Type*>(value));
  };
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::call(const utility::hashed_string& tag, const entity_type entity) -> void {
  if constexpr (has_meta_v<value_type>) {
    std::invoke(meta<value_type>{}, tag, get(entity));
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::pop(underlying_iterator first, underlying_iterator last) -> void {
  auto allocator = get_allocator();
  for (; first != last; ++first) {
    auto& element = _element_at(base_type::index(*first));

    if constexpr (component_traits::in_place_delete) {
      base_type::in_place_pop(first);
      allocator_traits::destroy(allocator, std::addressof(element));
    } else {
      auto& other = _element_at(base_type::size() - 1u);
      [[maybe_unused]] auto unused = std::exchange(element, std::move(other));
      allocator_traits::destroy(allocator, std::addressof(other));
      base_type::swap_and_pop(first);
    }
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::pop_all() -> void {
  auto allocator = get_allocator();

  for (auto first = base_type::begin(); !(first.index() < 0); ++first) {
    if constexpr (component_traits::in_place_delete) {
      if (*first != tombstone_entity) {
        base_type::in_place_pop(first);
        allocator_traits::destroy(allocator, std::addressof(_element_at(static_cast<size_type>(first.index()))));
      }
    } else {
      base_type::swap_and_pop(first);
      allocator_traits::destroy(allocator, std::addressof(_element_at(static_cast<size_type>(first.index()))));
    }
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::try_emplace([[maybe_unused]] const entity_type entity, [[maybe_unused]] const bool force_back) -> underlying_iterator {
  if constexpr (std::is_default_constructible_v<value_type>) {
    return _emplace_element(entity, force_back);
  } else {
    return base_type::end();
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::_swap_or_move([[maybe_unused]] const std::size_t from, [[maybe_unused]] const std::size_t to) -> void {
  static constexpr auto is_pinned_type = !(std::is_move_constructible_v<Type> && std::is_move_assignable_v<Type>);

  utility::assert_that((from + 1u) && !is_pinned_type, "Pinned type");

  if constexpr(!is_pinned_type) {
    if constexpr(component_traits::in_place_delete) {
      (base_type::operator[](to) == tombstone_entity) ? _move_to(from, to) : _swap_at(from, to);
    } else {
      _swap_at(from, to);
    }
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::_element_at(const std::size_t position) const -> const value_type& {
  return _container[position / component_traits::page_size][utility::fast_mod(position, component_traits::page_size)];
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::_element_at(const std::size_t position) -> value_type& {
  return const_cast<value_type&>(std::as_const(*this)._element_at(position));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::_assure_at_least(const std::size_t position) {
  const auto index = position / component_traits::page_size;

  if (index >= _container.size()) {
    auto current = _container.size();
    auto allocator = get_allocator();
    _container.resize(index + 1u, nullptr);

    try {
      for (const auto last = _container.size(); current < last; ++current) {
        _container[current] = allocator_traits::allocate(allocator, component_traits::page_size);
      }
    } catch (...) {
      _container.resize(current);
      throw;
    }
  }

  return _container[index] + utility::fast_mod(position, component_traits::page_size);
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
template<typename... Args>
requires (std::is_constructible_v<Type, Args...>)
auto basic_storage<Type, Entity, Allocator>::_emplace_element(const entity_type entity, const bool force_back, Args&&... args) {
  const auto iterator = base_type::try_emplace(entity, force_back);

  try {
    auto* element = std::to_address(_assure_at_least(static_cast<size_type>(iterator.index())));
    std::uninitialized_construct_using_allocator(element, get_allocator(), std::forward<Args>(args)...);
  } catch (...) {
    base_type::pop(iterator, iterator + 1u);
    throw;
  }

  return iterator;
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
auto basic_storage<Type, Entity, Allocator>::_shrink_to_size(const size_type size) -> void {
  const auto from = (size + component_traits::page_size - 1u) / component_traits::page_size;
  auto allocator = get_allocator();

  for (auto position = size, length = base_type::size(); position < length; ++position) {
    if constexpr (component_traits::in_place_delete) {
      if (base_type::data()[position] != tombstone_entity) {
        allocator_traits::destroy(allocator, std::addressof(_element_at(position)));
      }
    } else {
      allocator_traits::destroy(allocator, std::addressof(_element_at(position)));
    }
  }

  for (auto position = from, last = _container.size(); position < last; ++position) {
    allocator_traits::deallocate(allocator, _container[position], component_traits::page_size);
  }

  _container.resize(from);
  _container.shrink_to_fit();
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
void basic_storage<Type, Entity, Allocator>::_swap_at(const size_type lhs, const size_type rhs) {
  using std::swap;
  swap(_element_at(lhs), _element_at(rhs));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
void basic_storage<Type, Entity, Allocator>::_move_to(const size_type lhs, const size_type rhs) {
  auto& element = _element_at(lhs);
  auto allocator = get_allocator();
  std::uninitialized_construct_using_allocator(std::to_address(_assure_at_least(rhs)), allocator, std::move(element));
  allocator_traits::destroy(allocator, std::addressof(element));
}


template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
basic_storage<Type, Entity, Allocator>::basic_storage()
: basic_storage{allocator_type{}} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
basic_storage<Type, Entity, Allocator>::basic_storage(const allocator_type& allocator)
: base_type{storage_policy, allocator} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
basic_storage<Type, Entity, Allocator>::basic_storage(basic_storage&& other, const allocator_type& allocator)
: base_type{std::move(other), allocator} { }

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
[[nodiscard]] constexpr auto basic_storage<Type, Entity, Allocator>::get_allocator() const noexcept -> allocator_type {
  if constexpr(std::is_void_v<element_type> && !std::is_constructible_v<allocator_type, typename base_type::allocator_type>) {
    return allocator_type{};
  } else {
    return allocator_type{base_type::get_allocator()};
  }
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
auto basic_storage<Type, Entity, Allocator>::get([[maybe_unused]] const entity_type entity) const noexcept -> void {
  utility::assert_that(base_type::contains(entity), "Invalid entity");
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::get_as_tuple([[maybe_unused]] const entity_type entity) const noexcept -> std::tuple<> {
  utility::assert_that(base_type::contains(entity), "Invalid entity");
  return std::tuple{};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
template<typename... Args>
auto basic_storage<Type, Entity, Allocator>::emplace(const entity_type entity, Args&& ...) -> void {
  base_type::try_emplace(entity, false);
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
template<typename Function>
requires (std::is_invocable_v<Function>)
auto basic_storage<Type, Entity, Allocator>::patch([[maybe_unused]] const entity_type entity, Function&& function) -> void {
  utility::assert_that(base_type::contains(entity), "Invalid entity");
  std::invoke(std::forward<Function>(function));
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::each() noexcept -> iterable {
  return iterable{base_type::begin(), base_type::end()};
}

template<typename Type, typename Entity, memory::allocator_for<Type> Allocator>
requires (component_traits<Type, Entity>::page_size == 0u)
[[nodiscard]] auto basic_storage<Type, Entity, Allocator>::each() const noexcept -> const_iterable {
  return const_iterable{base_type::cbegin(), base_type::cend()};
}


template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_storage<Entity, Entity, Allocator>::basic_storage()
: base_type{allocator_type{}} { }

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_storage<Entity, Entity, Allocator>::basic_storage(const allocator_type& allocator)
: base_type{storage_policy, allocator},
  _placeholder{0u} {}

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_storage<Entity, Entity, Allocator>::basic_storage(basic_storage&& other) noexcept
: base_type{std::move(other)},
  _placeholder{other._placeholder} {}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::operator=(basic_storage&& other) noexcept -> basic_storage& {
  _placeholder = other._placeholder;
  base_type::operator=(std::move(other));
  return *this;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::get([[maybe_unused]] const entity_type entity) const noexcept -> void {
  utility::assert_that(base_type::index(entity) < base_type::free_list(), "The requested entity is not alive");
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
[[nodiscard]] auto basic_storage<Entity, Entity, Allocator>::get_as_tuple([[maybe_unused]] const entity_type entity) const noexcept -> std::tuple<> {
  utility::assert_that(base_type::index(entity) < base_type::free_list(), "The requested entity is not alive");
  return std::tuple{};
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::generate() -> entity_type {
  const auto index = base_type::free_list();
  // const auto entity = (length == base_type::size()) ? _next() : base_type::data()[length];
  const auto size = base_type::size();
  const auto entity = (index == size) ? _next() : base_type::data()[index];

  return *base_type::try_emplace(entity, true);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::generate(const entity_type hint) -> entity_type {
  if (hint != null_entity && hint != tombstone_entity) {
    if (const auto current = entity_traits::construct(entity_traits::to_entity(hint), base_type::current(hint)); current == tombstone_entity || !(base_type::index(current) < base_type::free_list())) {
      return *base_type::try_emplace(hint, true);
    }
  }

  return generate();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
template<typename Function>
requires (std::is_invocable_v<Function>)
auto basic_storage<Entity, Entity, Allocator>::patch([[maybe_unused]] const entity_type entity, Function&& function) -> void {
  utility::assert_that(base_type::index(entity) < base_type::free_list(), "The requested entity is not valid");
  std::invoke(std::forward<Function>(function));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
[[nodiscard]] auto basic_storage<Entity, Entity, Allocator>::each() noexcept -> iterable {
  return std::as_const(*this).each();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
[[nodiscard]] auto basic_storage<Entity, Entity, Allocator>::each() const noexcept -> const_iterable {
  const auto iterator = base_type::cend();
  const auto offset = static_cast<difference_type>(base_type::free_list());

  return const_iterable{iterator - offset, iterator};
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::pop_all() -> void {
  base_type::pop_all();
  _placeholder = {};
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::try_emplace(const entity_type hint, const bool) -> underlying_iterator {
  return base_type::find(generate(hint));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::_from_placeholder() noexcept {
  const auto entity = entity_traits::combine(static_cast<typename entity_traits::entity_type>(_placeholder), {});
  utility::assert_that(entity != null_entity, "No more entities available");
  _placeholder += static_cast<size_type>(entity != null_entity);
  return entity;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_storage<Entity, Entity, Allocator>::_next() noexcept {
  auto entity = _from_placeholder();

  while (base_type::current(entity) != entity_traits::to_version(tombstone_entity) && entity != null_entity) {
    entity = _from_placeholder();
  }

  return entity;
}

} // namespace sbx::ecs