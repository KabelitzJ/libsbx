// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/containers/ring_buffer.hpp
 *
 * @brief A fixed-capacity FIFO overwrite buffer on contiguous storage.
 *
 * @ingroup libsbx-containers
 */

#ifndef LIBSBX_CONTAINERS_RING_BUFFER_HPP_
#define LIBSBX_CONTAINERS_RING_BUFFER_HPP_

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include <libsbx/utility/assert.hpp>

namespace sbx::containers {

/**
 * @brief A fixed-capacity fifo overwrite buffer on contiguous storage.
 *
 * The vector grows up to the capacity, then the oldest element is overwritten
 * in place. Indexing and iteration are in logical order: index 0 is the oldest
 * element, size() - 1 the newest.
 */
template<typename Type>
class ring_buffer {

public:

  using value_type = Type;
  using size_type = std::size_t;

  class const_iterator {

  public:

    using iterator_category = std::forward_iterator_tag;
    using value_type = Type;
    using difference_type = std::ptrdiff_t;
    using pointer = const Type*;
    using reference = const Type&;

    const_iterator() = default;

    const_iterator(const ring_buffer* owner, size_type index)
    : _owner{owner},
      _index{index} { }

    auto operator*() const -> reference {
      return (*_owner)[_index];
    }

    auto operator->() const -> pointer {
      return &(**this);
    }

    auto operator++() -> const_iterator& {
      ++_index;

      return *this;
    }

    auto operator++(int) -> const_iterator {
      auto copy = *this;

      ++(*this);

      return copy;
    }

    auto operator==(const const_iterator& other) const -> bool = default;

  private:

    const ring_buffer* _owner{nullptr};
    size_type _index{0};

  }; // class const_iterator

  explicit ring_buffer(const size_type capacity)
  : _capacity{capacity} {
    utility::assert_that(capacity > 0u, "ring_buffer capacity must not be zero");

    _data.reserve(capacity);
  }

  auto push(const Type& value) -> void {
    emplace(value);
  }

  auto push(Type&& value) -> void {
    emplace(std::move(value));
  }

  template<typename... Args>
  auto emplace(Args&&... args) -> void {
    if (_data.size() < _capacity) {
      _data.emplace_back(std::forward<Args>(args)...);
    } else {
      _data[_next] = Type{std::forward<Args>(args)...};
    }

    _next = (_next + 1u) % _capacity;
  }

  [[nodiscard]] auto size() const noexcept -> size_type {
    return _data.size();
  }

  [[nodiscard]] auto capacity() const noexcept -> size_type {
    return _capacity;
  }

  [[nodiscard]] auto is_empty() const noexcept -> bool {
    return _data.empty();
  }

  [[nodiscard]] auto is_full() const noexcept -> bool {
    return _data.size() == _capacity;
  }

  auto clear() -> void {
    _data.clear();
    _next = 0u;
  }

  /**
   * @brief Logical indexing: 0 is the oldest element.
   */
  [[nodiscard]] auto operator[](const size_type index) const -> const Type& {
    return _data[_index(index)];
  }

  [[nodiscard]] auto operator[](const size_type index) -> Type& {
    return _data[_index(index)];
  }

  [[nodiscard]] auto begin() const -> const_iterator {
    return const_iterator{this, 0u};
  }

  [[nodiscard]] auto end() const -> const_iterator {
    return const_iterator{this, size()};
  }

private:

  auto _index(const size_type index) const -> size_type {
    utility::assert_that(index < _data.size(), "ring_buffer index out of range");

    const auto start = is_full() ? _next : 0u;

    return (start + index) % _capacity;
  }

  std::vector<Type> _data{};
  size_type _capacity;
  size_type _next{0u};

}; // class ring_buffer

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_RING_BUFFER_HPP_
