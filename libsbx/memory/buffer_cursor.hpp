// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_BUFFER_CURSOR_HPP_
#define LIBSBX_MEMORY_BUFFER_CURSOR_HPP_

#include <cstddef>

namespace sbx::memory {

/**
 * @brief A write cursor into a raw buffer: tracks a current position and advances by a count of
 * Type elements, without owning or bounds-checking the buffer.
 *
 * @tparam Type The pointee type the cursor is positioned in terms of; advance()'s amount is a
 * count of Type elements, not bytes (unless Type is std::byte).
 */
template<typename Type>
class buffer_cursor {

public:

  using value_type = Type;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using size_type = std::size_t;

  constexpr buffer_cursor(pointer ptr) noexcept
  : _current{ptr} {}

  constexpr auto get() noexcept -> pointer {
    return _current;
  }

  constexpr auto get() const noexcept -> const_pointer {
    return _current;
  }

  /** @return The cursor's new position, after advancing by amount Type elements. */
  constexpr auto advance(const size_type amount) noexcept -> pointer {
    return _current += amount;
  }

  constexpr auto operator+=(const size_type amount) noexcept -> buffer_cursor& {
    _current = advance(amount);

    return *this;
  }

  constexpr auto reset(pointer ptr) noexcept -> void {
    _current = ptr;
  }

private:

  pointer _current;

}; // class buffer_cursor

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_BUFFER_CURSOR_HPP_
