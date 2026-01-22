// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SPARSE_SET_HPP_
#define LIBSBX_SPARSE_SET_HPP_

#include <memory>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <functional>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/algorithm.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/memory/concepts.hpp>

#include <libsbx/ecs/detail/sparse_set_iterator.hpp>

namespace sbx::ecs {

enum class deletion_policy : std::uint8_t {
  swap_and_pop = 0u,
  in_place = 1u,
  swap_only = 2u,
  unspecified = swap_and_pop
}; // enum class deletion_policy

template<typename Entity, memory::allocator_for<Entity> Allocator = std::allocator<Entity>>
class basic_sparse_set {

  using allocator_traits = std::allocator_traits<Allocator>;

  using entity_traits = ecs::entity_traits<Entity>;

  using dense_storage_type = std::vector<Entity, Allocator>;
  using sparse_storage_type = std::vector<typename allocator_traits::pointer, typename allocator_traits::rebind_alloc<typename allocator_traits::pointer>>;

  inline static constexpr auto max_size = static_cast<std::size_t>(entity_traits::to_entity(null_entity));

public:

  using allocator_type = Allocator;
  using entity_type = entity_traits::value_type;
  using version_type = entity_traits::version_type;
  using pointer = typename dense_storage_type::pointer;
  using const_pointer = typename dense_storage_type::const_pointer;
  using reference = typename dense_storage_type::reference;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = detail::sparse_set_iterator<dense_storage_type>;
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  basic_sparse_set(const deletion_policy policy = deletion_policy::unspecified, const allocator_type& allocator = allocator_type{})
  : _dense{allocator},
    _sparse{allocator},
    _policy{policy},
    _head{_policy_to_head()} { }

  basic_sparse_set(const basic_sparse_set&) = delete;

  basic_sparse_set(basic_sparse_set&& other) noexcept
  : _dense{std::move(other._dense)},
    _sparse{std::move(other._sparse)},
    _policy{other._policy},
    _head{std::exchange(other._head, _policy_to_head())} { }

    basic_sparse_set(basic_sparse_set&& other, const allocator_type& allocator)
  : _dense{std::move(other._dense), allocator},
    _sparse{std::move(other._sparse), allocator},
    _policy{other._policy},
    _head{std::exchange(other._head, _policy_to_head())} { 
    utility::assert_that(allocator_type::is_always_equal::value || get_allocator() == other.get_allocator(), "Copying a sparse set is not allowed");
  }

  virtual ~basic_sparse_set() {
    _release_sparse_pages();
  }

  auto operator=(const basic_sparse_set& other) -> basic_sparse_set& = delete;

  auto operator=(basic_sparse_set&& other) noexcept -> basic_sparse_set& {
    utility::assert_that(allocator_type::is_always_equal::value || get_allocator() == other.get_allocator(), "Copying a sparse set is not allowed");

    swap(other);

    return *this;
  }

  auto swap(basic_sparse_set& other) noexcept -> void {
    using std::swap;

    swap(_sparse, other._sparse);
    swap(_dense, other._dense);
    swap(_policy, other._policy);
    swap(_head, other._head);
  }

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type {
    return _dense.get_allocator();
  }

  [[nodiscard]] auto policy() const noexcept -> deletion_policy {
    return _policy;
  }

  [[nodiscard]] auto free_list() const noexcept -> size_type {
    return _head;
  }

  virtual void reserve(const size_type capacity) {
    _dense.reserve(capacity);
  }

  [[nodiscard]] virtual auto capacity() const noexcept -> size_type {
    return _dense.capacity();
  }

  [[nodiscard]] auto extent() const noexcept -> size_type {
    return _sparse.size() * entity_type::page_size;
  }

  [[nodiscard]] auto size() const noexcept -> size_type {
    return _dense.size();
  }

  [[nodiscard]] auto is_empty() const noexcept -> bool {
    return _dense.empty();
  }

  [[nodiscard]] auto is_contiguous() const noexcept -> bool {
    return (_policy != deletion_policy::in_place) || (_head == max_size);
  }

  [[nodiscard]] auto data() const noexcept -> const_pointer {
    return _dense.data();
  }

  [[nodiscard]] auto data() noexcept -> pointer {
    return _dense.data();
  }

  auto bump(const entity_type entity) -> version_type {
    auto& element = _sparse_reference(entity);

    utility::assert_that(entity != null_entity && element != tombstone_entity, "Cannot set the required version");

    element = entity_traits::combine(entity_traits::to_integral(element), entity_traits::to_integral(entity));
    _dense[_entity_to_position(element)] = entity;

    return entity_traits::to_version(entity);
  }

  [[nodiscard]] auto begin() const noexcept -> iterator {
    const auto position = static_cast<difference_type>(_dense.size());
    return iterator{_dense, position};
  }

  [[nodiscard]] auto cbegin() const noexcept -> const_iterator {
    return begin();
  }

  [[nodiscard]] auto end() const noexcept -> iterator {
    return iterator{_dense, {}};
  }

  [[nodiscard]] auto cend() const noexcept -> const_iterator {
    return end();
  }

  [[nodiscard]] bool contains(const entity_type entity) const noexcept {
    constexpr auto capacity = entity_traits::entity_mask;
    constexpr auto mask = entity_traits::to_integral(null_entity) & ~capacity;

    const auto* element = _sparse_pointer(entity);

    return element && (((mask & entity_traits::to_integral(entity)) ^ entity_traits::to_integral(*element)) < capacity);
  }

  [[nodiscard]] auto current(const entity_type entity) const noexcept -> version_type {
    const auto* element = _sparse_pointer(entity);
    constexpr auto fallback = entity_traits::to_version(tombstone_entity);

    return element ? entity_traits::to_version(*element) : fallback;
  }

  [[nodiscard]] auto find(const entity_type entity) const noexcept -> const_iterator {
    return contains(entity) ? _to_iterator(entity) : end();
  }

  [[nodiscard]] auto index(const entity_type entity) const noexcept -> size_type {
    utility::assert_that(contains(entity), "Set does not contain entity");
    return _entity_to_position(_sparse_reference(entity));
  }

  auto erase(const entity_type entity) -> void {
    const auto it = _to_iterator(entity);
    pop(it, it + 1u);
  }

  auto remove(const entity_type entity) -> bool {
    if (!contains(entity)) {
      return false;
    }
    
    erase(entity);
    return true;
  }

  template<typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort(Compare compare, Sort sort = Sort{}, Args&&... args) -> void {
    const auto length = (_policy == deletion_policy::swap_only) ? _head : _dense.size();
    sort_n(length, std::move(compare), std::move(sort), std::forward<Args>(args)...);
  }

  template<typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort_n(const size_type length, Compare compare, Sort sort = Sort{}, Args&&... args) -> void {
    utility::assert_that((_policy != deletion_policy::in_place) || (_head == max_size), "Sorting with tombstones not allowed");
    utility::assert_that(!(length > _dense.size()), "Length exceeds the number of elements");

    sort(_dense.rend() - static_cast<difference_type>(length), _dense.rend(), std::move(compare), std::forward<Args>(args)...);

    for(auto position = 0u; position < length; ++position) {
      auto current = position;
      auto next = index(_dense[current]);

      while(current != next) {
        const auto idx = index(_dense[next]);
        const auto entity = _dense[current];

        _swap_or_move(next, idx);
        const auto element = static_cast<typename entity_traits::entity_type>(current);
        _sparse_reference(entity) = entity_traits::combine(element, entity_traits::to_integral(_dense[current]));
        current = std::exchange(next, idx);
      }
    }
  }

  auto clear() -> void {
    pop_all();
    _head = _policy_to_head();
    _dense.clear();
  }

  auto invoke(const utility::hashed_string& tag, const entity_type entity) -> void {
    if (contains(entity)) {
      call(tag, entity);
    }
  }

protected:

  using basic_iterator = iterator;

  virtual auto call([[maybe_unused]] const utility::hashed_string& tag, [[maybe_unused]] const entity_type entity) -> void {

  }

  void swap_only(const basic_iterator iterator) {
    utility::assert_that(_policy == deletion_policy::swap_only, "Deletion policy mismatch");

    const auto position = index(*iterator);
    bump(entity_traits::next(*iterator));
    _swap_at(position, _head -= (position < _head));
  }

  auto swap_and_pop(const basic_iterator iterator) -> void {
    utility::assert_that(_policy == deletion_policy::swap_and_pop, "Deletion policy mismatch");

    auto& self = _sparse_reference(*iterator);
    const auto entity = entity_traits::to_entity(self);
    _sparse_reference(_dense.back()) = entity_traits::combine(entity, entity_traits::to_integral(_dense.back()));
    _dense[static_cast<size_type>(entity)] = _dense.back();

    utility::assert_that((_dense.back() = null_entity, true), "unnecessary but it helps to detect nasty bugs");

    self = null_entity;
    _dense.pop_back();
  }

  auto in_place_pop(const basic_iterator iterator) -> void {
    utility::assert_that(_policy == deletion_policy::in_place, "Deletion policy mismatch");

    const auto position = _entity_to_position(std::exchange(_sparse_reference(*iterator), null_entity));
    _dense[position] = entity_traits::combine(static_cast<typename entity_traits::entity_type>(std::exchange(_head, position)), tombstone_entity);
  }

  virtual auto pop(basic_iterator first, basic_iterator last) -> void {
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

  virtual auto pop_all() -> void {
    for (auto&& element : _dense) {
      _sparse_reference(element) = null_entity;
    }

    _head = _policy_to_head();
    _dense.clear();
  }

  virtual auto try_emplace(const entity_type entity, const bool force_back) -> basic_iterator {
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

private:

  [[nodiscard]] virtual auto get_at(const std::size_t) const -> const void* {
    return nullptr;
  }

  virtual auto _swap_or_move([[maybe_unused]] const std::size_t from, [[maybe_unused]] const std::size_t to) -> void {
    utility::assert_that((_policy != deletion_policy::swap_only) || ((from < _head) == (to < _head)), "Cross swapping is not supported");
  }

  [[nodiscard]] auto _policy_to_head() const noexcept -> size_type {
    return max_size * static_cast<size_type>(_policy != deletion_policy::swap_only);
  }

  [[nodiscard]] auto _entity_to_position(const entity_type entity) const noexcept {
    return static_cast<size_type>(entity_traits::to_entity(entity));
  }

  [[nodiscard]] auto _position_to_page(const size_type position) const noexcept {
    return static_cast<size_type>(position / entity_traits::page_size);
  }

  [[nodiscard]] auto _sparse_pointer(const entity_type entity) const -> pointer {
    const auto position = _entity_to_position(entity);
    const auto page = _position_to_page(position);

    return (page < _sparse.size() && _sparse[page]) ? (_sparse[page] + utility::fast_mod(position, entity_traits::page_size)) : nullptr;
  }

  [[nodiscard]] auto _sparse_reference(const entity_type entity) const -> reference {
    utility::assert_that(_sparse_pointer(entity), "Invalid element");

    const auto position = _entity_to_position(entity);
    const auto page = _position_to_page(position);

    return _sparse[page][utility::fast_mod(position, entity_traits::page_size)];
  }

  void _release_sparse_pages() {
    auto page_allocator = _dense.get_allocator();

    for (auto&& page : _sparse) {
      if (page != nullptr) {
        std::destroy(page, page + entity_traits::page_size);
        allocator_traits::deallocate(page_allocator, page, entity_traits::page_size);
        page = nullptr;
      }
    }
  }

  [[nodiscard]] auto _to_iterator(const entity_type entity) const noexcept -> iterator {
    return --(end() - static_cast<difference_type>(index(entity)));
  }

  [[nodiscard]] auto _assure_at_least(const entity_type entity) -> reference {
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

  auto _swap_at(const size_type lhs, const size_type rhs) -> void {
    auto& from = _dense[lhs];
    auto& to = _dense[rhs];

    _sparse_reference(from) = entity_traits::combine(static_cast<typename entity_traits::entity_type>(rhs), entity_traits::to_integral(from));
    _sparse_reference(to) = entity_traits::combine(static_cast<typename entity_traits::entity_type>(lhs), entity_traits::to_integral(to));

    std::swap(from, to);
  }

  dense_storage_type _dense;
  sparse_storage_type _sparse;
  deletion_policy _policy;
  size_type _head;

}; // class sparse_set

} // namespace sbx::ecs

#endif // LIBSBX_SPARSE_SET_HPP_
