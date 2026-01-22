// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_ITERABLE_ADAPTOR_HPP_
#define LIBSBX_MEMORY_ITERABLE_ADAPTOR_HPP_

#include <concepts>
#include <iterator>

namespace sbx::memory {

template<std::forward_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator>
class iterable_adaptor final {

public:

  using value_type = typename std::iterator_traits<Iterator>::value_type;
  using iterator = Iterator;
  using sentinel = Sentinel;

  constexpr iterable_adaptor() noexcept(std::is_nothrow_default_constructible_v<iterator> && std::is_nothrow_default_constructible_v<sentinel>)
  : _first{},
    _last{} { }

  constexpr iterable_adaptor(iterator from, sentinel to) noexcept(std::is_nothrow_move_constructible_v<iterator> && std::is_nothrow_move_constructible_v<sentinel>)
  : _first{std::move(from)},
    _last{std::move(to)} { }

  [[nodiscard]] constexpr auto begin() const noexcept -> iterator {
    return _first;
  }

  [[nodiscard]] constexpr auto end() const noexcept -> sentinel {
    return _last;
  }

  [[nodiscard]] constexpr auto cbegin() const noexcept -> iterator {
    return begin();
  }

  [[nodiscard]] constexpr auto cend() const noexcept -> sentinel {
    return end();
  }

private:

  iterator _first;
  sentinel _last;

}; // struct iterable_adaptor

template<typename Type>
class input_iterator_pointer final {

public:

  using value_type = Type;
  using pointer = Type *;
  using reference = Type &;

  constexpr input_iterator_pointer(value_type&& value) noexcept(std::is_nothrow_move_constructible_v<value_type>)
  : _value{std::move(value)} {}

  [[nodiscard]] constexpr auto operator->() noexcept -> pointer {
    return std::addressof(_value);
  }

  [[nodiscard]] constexpr auto operator*() noexcept -> reference {
    return _value;
  }

private:

  value_type _value;

}; // struct input_iterator_pointer

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ITERABLE_ADAPTOR_HPP_
