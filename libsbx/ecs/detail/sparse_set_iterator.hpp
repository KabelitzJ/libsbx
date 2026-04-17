// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ECS_DETAIL_SPARSE_SET_ITERATOR_HPP_
#define LIBSBX_ECS_DETAIL_SPARSE_SET_ITERATOR_HPP_

#include <utility>

namespace sbx::ecs::detail {

template<typename Container>
class sparse_set_iterator final {

public:

  using value_type = typename Container::value_type;
  using pointer = typename Container::const_pointer;
  using reference = typename Container::const_reference;
  using difference_type = typename Container::difference_type;
  using iterator_category = std::random_access_iterator_tag;

  constexpr sparse_set_iterator() noexcept
  : _dense{},
    _offset{} {}

  constexpr sparse_set_iterator(const Container& dense, const difference_type offset) noexcept
  : _dense{&dense},
    _offset{offset} {}

  constexpr auto operator++() noexcept -> sparse_set_iterator& {
    --_offset;
    return *this;
  }

  constexpr auto operator++(int) noexcept -> sparse_set_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  constexpr auto operator--() noexcept -> sparse_set_iterator& {
    ++_offset;
    return *this;
  }

  constexpr sparse_set_iterator operator--(int) noexcept {
    const auto original = *this;
    --(*this);
    return original;
  }

  constexpr auto operator+=(const difference_type value) noexcept -> sparse_set_iterator& {
    _offset -= value;
    return *this;
  }

  constexpr auto operator+(const difference_type value) const noexcept -> sparse_set_iterator {
    auto copy = *this;
    return (copy += value);
  }

  constexpr auto operator-=(const difference_type value) noexcept -> sparse_set_iterator& {
    return (*this += -value);
  }

  constexpr auto operator-(const difference_type value) const noexcept -> sparse_set_iterator {
    return (*this + -value);
  }

  [[nodiscard]] constexpr auto operator[](const difference_type value) const noexcept -> reference {
    return (*_dense)[static_cast<typename Container::size_type>(index() - value)];
  }

  [[nodiscard]] constexpr auto operator->() const noexcept -> pointer {
    return std::addressof(operator[](0));
  }

  [[nodiscard]] constexpr auto operator*() const noexcept -> reference {
    return operator[](0);
  }

  [[nodiscard]] constexpr auto data() const noexcept -> pointer {
    return _dense ? _dense->data() : nullptr;
  }

  [[nodiscard]] constexpr auto index() const noexcept -> difference_type {
    return _offset - 1;
  }

private:

  const Container* _dense;
  difference_type _offset;

}; // struct sparse_set_iterator

template<typename Container>
[[nodiscard]] constexpr auto operator-(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> std::ptrdiff_t {
  return rhs.index() - lhs.index();
}

template<typename Container>
[[nodiscard]] constexpr auto operator==(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return lhs.index() == rhs.index();
}

template<typename Container>
[[nodiscard]] constexpr auto operator!=(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return !(lhs == rhs);
}

template<typename Container>
[[nodiscard]] constexpr auto operator<(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return lhs.index() > rhs.index();
}

template<typename Container>
[[nodiscard]] constexpr auto operator>(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return rhs < lhs;
}

template<typename Container>
[[nodiscard]] constexpr auto operator<=(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return !(lhs > rhs);
}

template<typename Container>
[[nodiscard]] constexpr auto operator>=(const sparse_set_iterator<Container> &lhs, const sparse_set_iterator<Container> &rhs) noexcept -> bool {
  return !(lhs < rhs);
}

} // namespace sbx::ecs

#endif // LIBSBX_ECS_DETAIL_SPARSE_SET_ITERATOR_HPP_
