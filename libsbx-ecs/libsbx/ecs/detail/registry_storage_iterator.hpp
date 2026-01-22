// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ECS_DETAIL_REGISTRY_STORAGE_ITERATOR_HPP_
#define LIBSBX_ECS_DETAIL_REGISTRY_STORAGE_ITERATOR_HPP_

#include <iterator>
#include <type_traits>

#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/memory/iterable_adaptor.hpp>

namespace sbx::ecs::detail {

template<typename Iterator>
class registry_storage_iterator final {

  template<typename Other>
  friend class registry_storage_iterator;

  template<typename Lhs, typename Rhs>
  friend constexpr auto operator-(const registry_storage_iterator<Lhs>&, const registry_storage_iterator<Rhs>&) noexcept -> std::ptrdiff_t;

  template<typename Lhs, typename Rhs>
  friend constexpr auto operator==(const registry_storage_iterator<Lhs>&, const registry_storage_iterator<Rhs>&) noexcept -> bool;

  using mapped_type = std::remove_reference_t<decltype(std::declval<Iterator>()->second)>;

public:

  using value_type = std::pair<std::uint32_t, utility::constness_as_t<typename mapped_type::element_type, mapped_type>&>;
  using pointer = memory::input_iterator_pointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using iterator_concept = std::random_access_iterator_tag;

  constexpr registry_storage_iterator() noexcept
  : _iterator{} { }

  constexpr registry_storage_iterator(Iterator iter) noexcept
  : _iterator{iter} { }

  template<typename Other>
  requires (!std::is_same_v<Iterator, Other> && std::is_constructible_v<Iterator, Other>)
  constexpr registry_storage_iterator(const registry_storage_iterator<Other>& other) noexcept
  : registry_storage_iterator{other._iterator} { }

  constexpr auto operator++() noexcept -> registry_storage_iterator& {
    ++_iterator;
    return *this;
  }

  constexpr auto operator++(int) noexcept -> registry_storage_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  constexpr auto operator--() noexcept -> registry_storage_iterator& {
      --_iterator;
      return *this;
  }

  constexpr auto operator--(int) noexcept -> registry_storage_iterator {
    const auto original = *this;
    --(*this);
    return original;
  }

  constexpr auto operator+=(const difference_type value) noexcept -> registry_storage_iterator& {
    _iterator += value;
    return *this;
  }

  constexpr auto operator+(const difference_type value) const noexcept -> registry_storage_iterator {
    auto copy = *this;
    return (copy += value);
  }

  constexpr auto operator-=(const difference_type value) noexcept -> registry_storage_iterator& {
    return (*this += -value);
  }

  constexpr auto operator-(const difference_type value) const noexcept -> registry_storage_iterator {
    return (*this + -value);
  }

  [[nodiscard]] constexpr auto operator[](const difference_type value) const noexcept -> reference {
    return {_iterator[value].first, *_iterator[value].second};
  }

  [[nodiscard]] constexpr auto operator*() const noexcept -> reference {
    return operator[](0);
  }

  [[nodiscard]] constexpr auto operator->() const noexcept -> pointer {
    return operator*();
  }

private:

  Iterator _iterator;

}; // class registry_storage_iterator

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator-(const registry_storage_iterator<Lhs>& lhs, const registry_storage_iterator<Rhs>& rhs) noexcept -> std::ptrdiff_t {
  return lhs._iterator - rhs._iterator;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator==(const registry_storage_iterator<Lhs>& lhs, const registry_storage_iterator<Rhs>& rhs) noexcept -> bool {
  return lhs._iterator == rhs._iterator;
}

} // namespace sbx::ecs::detail

#endif // LIBSBX_ECS_DETAIL_REGISTRY_STORAGE_ITERATOR_HPP_
