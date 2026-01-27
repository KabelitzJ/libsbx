// SPDX-License-Identifier: MIT
#include <libsbx/ecs/sparse_set.hpp>

namespace sbx::ecs {

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_sparse_set<Entity, Allocator>::basic_sparse_set(const deletion_policy policy, const allocator_type& allocator)
: _dense{allocator},
  _sparse{allocator},
  _policy{policy},
  _head{_policy_to_head()} { }

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_sparse_set<Entity, Allocator>::basic_sparse_set(basic_sparse_set&& other) noexcept
: _dense{std::move(other._dense)},
  _sparse{std::move(other._sparse)},
  _policy{other._policy},
  _head{std::exchange(other._head, _policy_to_head())} { }

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_sparse_set<Entity, Allocator>::basic_sparse_set(basic_sparse_set&& other, const allocator_type& allocator)
: _dense{std::move(other._dense), allocator},
  _sparse{std::move(other._sparse), allocator},
  _policy{other._policy},
  _head{std::exchange(other._head, _policy_to_head())} { 
  utility::assert_that(allocator_type::is_always_equal::value || get_allocator() == other.get_allocator(), "Copying a sparse set is not allowed");
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
basic_sparse_set<Entity, Allocator>::~basic_sparse_set() {
  _release_sparse_pages();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::operator=(basic_sparse_set&& other) noexcept -> basic_sparse_set& {
  utility::assert_that(allocator_type::is_always_equal::value || get_allocator() == other.get_allocator(), "Copying a sparse set is not allowed");

  swap(other);

  return *this;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::swap(basic_sparse_set& other) noexcept -> void {
  using std::swap;

  swap(_sparse, other._sparse);
  swap(_dense, other._dense);
  swap(_policy, other._policy);
  swap(_head, other._head);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
constexpr auto basic_sparse_set<Entity, Allocator>::get_allocator() const noexcept -> allocator_type {
  return _dense.get_allocator();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::policy() const noexcept -> deletion_policy {
  return _policy;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::free_list() const noexcept -> size_type {
  return _head;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
void basic_sparse_set<Entity, Allocator>::reserve(const size_type capacity) {
  _dense.reserve(capacity);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::capacity() const noexcept -> size_type {
  return _dense.capacity();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::extent() const noexcept -> size_type {
  return _sparse.size() * entity_type::page_size;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::size() const noexcept -> size_type {
  return _dense.size();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::is_empty() const noexcept -> bool {
  return _dense.empty();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::is_contiguous() const noexcept -> bool {
  return (_policy != deletion_policy::in_place) || (_head == max_size);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::data() const noexcept -> const_pointer {
  return _dense.data();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::data() noexcept -> pointer {
  return _dense.data();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::bump(const entity_type entity) -> version_type {
  auto& element = _sparse_reference(entity);

  utility::assert_that(entity != null_entity && element != tombstone_entity, "Cannot set the required version");

  element = entity_traits::combine(entity_traits::to_integral(element), entity_traits::to_integral(entity));
  _dense[_entity_to_position(element)] = entity;

  return entity_traits::to_version(entity);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::begin() const noexcept -> iterator {
  const auto position = static_cast<difference_type>(_dense.size());
  return iterator{_dense, position};
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::cbegin() const noexcept -> const_iterator {
  return begin();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::end() const noexcept -> iterator {
  return iterator{_dense, {}};
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::cend() const noexcept -> const_iterator {
  return end();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
bool basic_sparse_set<Entity, Allocator>::contains(const entity_type entity) const noexcept {
  constexpr auto capacity = entity_traits::entity_mask;
  constexpr auto mask = entity_traits::to_integral(null_entity) & ~capacity;

  const auto* element = _sparse_pointer(entity);

  return element && (((mask & entity_traits::to_integral(entity)) ^ entity_traits::to_integral(*element)) < capacity);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::current(const entity_type entity) const noexcept -> version_type {
  const auto* element = _sparse_pointer(entity);
  constexpr auto fallback = entity_traits::to_version(tombstone_entity);

  return element ? entity_traits::to_version(*element) : fallback;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::find(const entity_type entity) const noexcept -> const_iterator {
  return contains(entity) ? _to_iterator(entity) : end();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::index(const entity_type entity) const noexcept -> size_type {
  utility::assert_that(contains(entity), "Set does not contain entity");
  return _entity_to_position(_sparse_reference(entity));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::erase(const entity_type entity) -> void {
  const auto it = _to_iterator(entity);
  pop(it, it + 1u);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::remove(const entity_type entity) -> bool {
  if (!contains(entity)) {
    return false;
  }
  
  erase(entity);
  return true;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::clear() -> void {
  pop_all();
  _head = _policy_to_head();
  _dense.clear();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::invoke(const utility::hashed_string& tag, const entity_type entity) -> void {
  if (contains(entity)) {
    call(tag, entity);
  }
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::call([[maybe_unused]] const utility::hashed_string& tag, [[maybe_unused]] const entity_type entity) -> void {

}

template<typename Entity, memory::allocator_for<Entity> Allocator>
void basic_sparse_set<Entity, Allocator>::swap_only(const basic_iterator iterator) {
  utility::assert_that(_policy == deletion_policy::swap_only, "Deletion policy mismatch");

  const auto position = index(*iterator);
  bump(entity_traits::next(*iterator));
  _swap_at(position, _head -= (position < _head));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::swap_and_pop(const basic_iterator iterator) -> void {
  utility::assert_that(_policy == deletion_policy::swap_and_pop, "Deletion policy mismatch");

  auto& self = _sparse_reference(*iterator);
  const auto entity = entity_traits::to_entity(self);

  _sparse_reference(_dense.back()) = entity_traits::combine(entity, entity_traits::to_integral(_dense.back()));
  _dense[static_cast<size_type>(entity)] = _dense.back();

  utility::assert_that((_dense.back() = null_entity, true), "unnecessary but it helps to detect nasty bugs");

  self = null_entity;
  _dense.pop_back();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::in_place_pop(const basic_iterator iterator) -> void {
  utility::assert_that(_policy == deletion_policy::in_place, "Deletion policy mismatch");

  const auto position = _entity_to_position(std::exchange(_sparse_reference(*iterator), null_entity));

  _dense[position] = entity_traits::combine(static_cast<typename entity_traits::entity_type>(std::exchange(_head, position)), tombstone_entity);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::pop(basic_iterator first, basic_iterator last) -> void {
  if (first == last) {
    return;
  }

  switch (_policy) {
    case deletion_policy::swap_and_pop: {
      for (; first != last; ++first) {
        swap_and_pop(first);
      }
      break;
    }
    case deletion_policy::in_place: {
      for (; first != last; ++first) {
        in_place_pop(first);
      }
      break;
    }
    case deletion_policy::swap_only: {
      for (; first != last; ++first) {
        swap_only(first);
      }
      break;
    }
  }
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::pop_all() -> void {
  for (auto&& element : _dense) {
    _sparse_reference(element) = null_entity;
  }

  _head = _policy_to_head();
  _dense.clear();
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::try_emplace(const entity_type entity, const bool force_back) -> basic_iterator {
  utility::assert_that(entity != null_entity && entity != tombstone_entity, "Invalid element");

  auto& element = _assure_at_least(entity);
  auto position = size();

  switch(_policy) {
    case deletion_policy::in_place: {
      if(_head != max_size && !force_back) {
        position = _head;
        utility::assert_that(element == null_entity, "Slot not available");
        element = entity_traits::combine(static_cast<typename entity_traits::entity_type>(_head), entity_traits::to_integral(entity));
        _head = _entity_to_position(std::exchange(_dense[position], entity));
        break;
      }
      [[fallthrough]];
    }
    case deletion_policy::swap_and_pop: {
      _dense.push_back(entity);
      utility::assert_that(element == null_entity, "Slot not available");
      element = entity_traits::combine(static_cast<typename entity_traits::entity_type>(_dense.size() - 1u), entity_traits::to_integral(entity));
      break;
    }
    case deletion_policy::swap_only: {
      if(element == null_entity) {
        _dense.push_back(entity);
        element = entity_traits::combine(static_cast<typename entity_traits::entity_type>(_dense.size() - 1u), entity_traits::to_integral(entity));
      } else {
        utility::assert_that(!(_entity_to_position(element) < _head), "Slot not available");
        bump(entity);
      }

      position = _head++;
      _swap_at(_entity_to_position(element), position);
      break;
    }
    }

  return --(end() - static_cast<difference_type>(position));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::get_at(const std::size_t) const -> const void* {
  return nullptr;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_swap_or_move([[maybe_unused]] const std::size_t from, [[maybe_unused]] const std::size_t to) -> void {
  utility::assert_that((_policy != deletion_policy::swap_only) || ((from < _head) == (to < _head)), "Cross swapping is not supported");
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_policy_to_head() const noexcept -> size_type {
  return max_size * static_cast<size_type>(_policy != deletion_policy::swap_only);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_entity_to_position(const entity_type entity) const noexcept {
  return static_cast<size_type>(entity_traits::to_entity(entity));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_position_to_page(const size_type position) const noexcept {
  return static_cast<size_type>(position / entity_traits::page_size);
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_sparse_pointer(const entity_type entity) const -> pointer {
  const auto position = _entity_to_position(entity);
  const auto page = _position_to_page(position);

  return (page < _sparse.size() && _sparse[page]) ? (_sparse[page] + utility::fast_mod(position, entity_traits::page_size)) : nullptr;
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_sparse_reference(const entity_type entity) const -> reference {
  utility::assert_that(_sparse_pointer(entity), "Invalid element");

  const auto position = _entity_to_position(entity);
  const auto page = _position_to_page(position);

  return _sparse[page][utility::fast_mod(position, entity_traits::page_size)];
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
void basic_sparse_set<Entity, Allocator>::_release_sparse_pages() {
  auto page_allocator = _dense.get_allocator();

  for (auto&& page : _sparse) {
    if (page != nullptr) {
      std::destroy(page, page + entity_traits::page_size);
      allocator_traits::deallocate(page_allocator, page, entity_traits::page_size);
      page = nullptr;
    }
  }
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_to_iterator(const entity_type entity) const noexcept -> iterator {
  return --(end() - static_cast<difference_type>(index(entity)));
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_assure_at_least(const entity_type entity) -> reference {
  const auto position = _entity_to_position(entity);
  const auto page = _position_to_page(position);

  if (page >= _sparse.size()) {
    _sparse.resize(page + 1u, nullptr);
  }

  if (!_sparse[page]) {
    constexpr auto init = null_entity;
    auto page_allocator = _dense.get_allocator();

    _sparse[page] = allocator_traits::allocate(page_allocator, entity_traits::page_size);

    std::uninitialized_fill(_sparse[page], _sparse[page] + entity_traits::page_size, init);
  }

  return _sparse[page][utility::fast_mod(position, entity_traits::page_size)];
}

template<typename Entity, memory::allocator_for<Entity> Allocator>
auto basic_sparse_set<Entity, Allocator>::_swap_at(const size_type lhs, const size_type rhs) -> void {
  auto& from = _dense[lhs];
  auto& to = _dense[rhs];

  _sparse_reference(from) = entity_traits::combine(static_cast<typename entity_traits::entity_type>(rhs), entity_traits::to_integral(from));
  _sparse_reference(to) = entity_traits::combine(static_cast<typename entity_traits::entity_type>(lhs), entity_traits::to_integral(to));

  std::swap(from, to);
}

} // namespace sbx::ecs
