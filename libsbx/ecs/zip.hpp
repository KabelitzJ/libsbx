// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ZIP_HPP_
#define LIBSBX_ZIP_HPP_

#include <type_traits>
#include <concepts>
#include <vector>
#include <utility>

namespace sbx::ecs {

namespace detail {

template<std::input_iterator Iterator>
using select_access_type_for = std::conditional_t<
  std::is_same_v<Iterator, std::vector<bool>::iterator> || 
  std::is_same_v<Iterator, std::vector<bool>::const_iterator>,
  typename Iterator::value_type,
  typename Iterator::reference
>;

template<typename... Args, std::size_t ... Index>
auto any_match_impl(const std::tuple<Args...>& lhs, const std::tuple<Args...>& rhs, std::index_sequence<Index...>) -> bool {
  auto result = false;
  result = (... || (std::get<Index>(lhs) == std::get<Index>(rhs)));
  return result;
}

template<typename... Args>
auto any_match(const std::tuple<Args...>& lhs, const std::tuple<Args...>& rhs) -> bool {
  return any_match_impl(lhs, rhs, std::index_sequence_for<Args...>{});
}

template<typename... Iterators>
class zip_iterator {
public:

  using value_type = std::tuple<select_access_type_for<Iterators>...>;

  zip_iterator() = delete;

  zip_iterator(Iterators&&... iterators)
  : _iterators {std::forward<Iterators>(iterators)...} { }

  auto operator++() -> zip_iterator& {
    std::apply([](auto&&... iterators){ ((iterators += 1), ...); }, _iterators);
    return *this;
  }

  auto operator++(int) -> zip_iterator  {
    auto copy = *this;
    ++(*this);
    return copy;
  }

  auto operator==(const zip_iterator& other) {
    return any_match(_iterators, other._iterators);
  }

  auto operator*() -> value_type {
    return std::apply([](auto&&... iterators){ 
      return value_type(*iterators...); 
    }, _iterators);
  }

private:

  std::tuple<Iterators...> _iterators;

};

template<typename Type>
using select_iterator_for = std::conditional_t<
  std::is_const_v<std::remove_reference_t<Type>>, 
  typename std::decay_t<Type>::const_iterator,
  typename std::decay_t<Type>::iterator
>;

template<typename... Containers>
class zipper {

public:

  using iterator = zip_iterator<select_iterator_for<Containers>...>;

  template<typename... Args>
  zipper(Args&&... containers)
  : _containers{std::forward<Args>(containers)...} { }

  auto begin() -> iterator {
    return std::apply([](auto&& ... containers){ 
      return iterator(std::begin(containers)...); 
    }, _containers);
  }

  auto end() -> iterator {
    return std::apply([](auto&& ... containers){ 
      return iterator(std::end(containers)...); 
    }, _containers);
  }

private:

  std::tuple<Containers...> _containers;

}; // class zipper 

} // namespace detail

template<typename... Containers>
auto zip(Containers&&... containers) {
  return detail::zipper<Containers...>{std::forward<Containers>(containers)...};
}

} // namespace sbx::ecs

#endif // LIBSBX_ZIP_HPP_
