#ifndef LIBSBX_MEMORY_BUFFER_CURSOR_HPP_
#define LIBSBX_MEMORY_BUFFER_CURSOR_HPP_

namespace sbx::memory {

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

}; // lass buffer_cursor

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_BUFFER_CURSOR_HPP_