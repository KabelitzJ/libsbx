// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_STATIC_VECTOR_HPP_
#define LIBSBX_CONTAINERS_STATIC_VECTOR_HPP_

#include <memory>
#include <type_traits>
#include <ranges>
#include <array>

#include <fmt/format.h>

#include <libsbx/utility/assert.hpp>

#include <libsbx/memory/aligned_storage.hpp>

namespace sbx::containers {

/**
 * @brief static_vector implementation inspired by https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0843r2.html
 * 
 * @tparam Type
 * @tparam Capacity
 */
template<typename Type, std::size_t Capacity>
class static_vector {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using size_type = std::size_t;
  using iterator = pointer;
  using const_iterator = const_pointer;

  constexpr static_vector() noexcept
  : _size{0u} { }

  constexpr static_vector(const static_vector& other) noexcept
  : _size{other._size} {
    for (auto i : std::views::iota(0u, _size)) {
      std::construct_at(_ptr(i), other[i]);
    }
  }

  constexpr static_vector(static_vector&& other) noexcept
  : _size{other._size} {
    for (auto i : std::views::iota(0u, _size)) {
      std::construct_at(_ptr(i), std::move(other[i]));
    }
  }

  constexpr static_vector(std::initializer_list<value_type> values) noexcept
  : _size{values.size()} {
    utility::assert_that(_size <= Capacity, "initializer list size exceeds capacity");
    
    for (auto value : values) {
      if constexpr (std::is_move_constructible_v<Type>) {
        push_back(std::move(value));
      } else {
        push_back(value);
      }
    }
  }

  constexpr ~static_vector() noexcept {
    clear();
  }

  constexpr auto operator=(static_vector other) noexcept -> static_vector& {
    if (this != &other) {
      other.swap(*this);
    }
    
    return *this;
  }

  constexpr auto size() const noexcept -> size_type {
    return _size;
  }

  auto capacity() const noexcept -> size_type {
    return Capacity;
  }

  constexpr auto is_empty() const noexcept -> bool {
    return _size == 0u;
  }

  constexpr auto is_full() const noexcept -> bool {
    return _size == Capacity;
  }

  constexpr auto begin() noexcept -> iterator {
    return _ptr(0u);
  }

  constexpr auto begin() const noexcept -> const_iterator {
    return _ptr(0u);
  }

  constexpr auto cbegin() const noexcept -> const_iterator {
    return begin();
  }

  constexpr auto end() noexcept -> iterator {
    return _ptr(_size);
  }

  constexpr auto end() const noexcept -> const_iterator {
    return _ptr(_size);
  }

  constexpr auto cend() const noexcept -> const_iterator {
    return end();
  }

  constexpr auto front() noexcept -> reference {
    return *begin();
  }

  constexpr auto front() const noexcept -> const_reference {
    return *begin();
  }

  constexpr auto back() noexcept -> reference {
    return *std::prev(end());
  }

  constexpr auto back() const noexcept -> const_reference {
    return *std::prev(end());
  }

  constexpr auto operator[](const size_type index) noexcept -> reference {
    return *_ptr(index);
  }

  constexpr auto operator[](const size_type index) const noexcept -> const_reference {
    return *_ptr(index);
  }

  constexpr auto at(const size_type index) -> reference {
    utility::assert_that(index < _size, "index is out of range");
    
    return *_ptr(index);
  }

  constexpr auto at(const size_type index) const -> const_reference {
    utility::assert_that(index < _size, "index is out of range");
    
    return *_ptr(index);
  }

  constexpr auto data() noexcept -> pointer {
    return _ptr(0u);
  }

  constexpr auto data() const noexcept -> const_pointer {
    return _ptr(0u);
  }

  constexpr auto push_back(const value_type& value) noexcept -> void {
    if (is_full()) {
      return;
    }
    
    std::construct_at(_ptr(_size), value);
    ++_size;
  }

  constexpr auto push_back(value_type&& value) noexcept -> void {
    if (is_full()) {
      return;
    }
    
    std::construct_at(_ptr(_size), std::move(value));
    ++_size;
  }

  template<typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  constexpr auto emplace_back(Args&&... args) noexcept -> void {
    if (is_full()) {
      return;
    }
    
    std::construct_at(_ptr(_size), std::forward<Args>(args)...);
    ++_size;
  }

  constexpr auto pop_back() noexcept -> void {
    if (is_empty()) {
      return;
    }
    
    std::destroy_at(std::prev(end()));
    --_size;
  }

  constexpr auto clear() noexcept -> void {
    for (auto i : std::views::iota(0u, _size)) {
      std::destroy_at(_ptr(i));
    }
    
    _size = 0u;
  }

  constexpr auto swap(static_vector& other) -> void {
    using std::swap;

    swap(_size, other._size);
    swap(_buffer, other._buffer);
  }

private:

  constexpr auto _ptr(const size_type index) noexcept -> pointer {
    // utility::assert_that(index < Capacity, "index is out of range");
    return std::launder(reinterpret_cast<pointer>(_buffer.data() + index));
  }

  constexpr auto _ptr(const size_type index) const noexcept -> const_pointer {
    // utility::assert_that(index < Capacity, "index is out of range");
    return std::launder(reinterpret_cast<const_pointer>(_buffer.data() + index));
  }

  size_type _size;
  std::array<memory::storage_for_t<Type>, Capacity> _buffer;

}; // class static_vector

template<typename Type, std::size_t Capacity>
auto operator==(const static_vector<Type, Capacity>& lhs, const static_vector<Type, Capacity>& rhs) -> bool {
  return std::ranges::equal(lhs, rhs);
}

template<typename Type, std::size_t Capacity>
auto swap(static_vector<Type, Capacity>& lhs, static_vector<Type, Capacity>& rhs) -> void {
  lhs.swap(rhs);
}

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_STATIC_VECTOR_HPP_
