// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ECS_DETAIL_VIEW_ITERATOR_HPP_
#define LIBSBX_ECS_DETAIL_VIEW_ITERATOR_HPP_

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/type_list.hpp>

#include <libsbx/memory/iterable_adaptor.hpp>

#include <libsbx/ecs/sparse_set.hpp>
#include <libsbx/ecs/entity.hpp>

namespace sbx::ecs::detail {

template<typename... Type>
static constexpr bool tombstone_check_v = ((sizeof...(Type) == 1u) && ... && (Type::storage_policy == deletion_policy::in_place));

// template<typename Type>
// requires (std::is_same_v<std::remove_const_t<std::remove_reference_t<Type>>, Type>)
// auto view_placeholder() -> const Type* {
//   static const auto placeholder = Type{};
//   return &placeholder;
// }

template<std::forward_iterator Iterator, typename Entity>
[[nodiscard]] auto all_of(Iterator first, const Iterator last, const Entity entity) noexcept -> bool {
  for (; (first != last) && (*first)->contains(entity); ++first) { }
  return first == last;
}

template<std::forward_iterator Iterator, typename Entity>
[[nodiscard]] auto none_of(Iterator first, const Iterator last, const Entity entity) noexcept -> bool {
  for (; (first != last) && !(*first)->contains(entity); ++first) { }
  return first == last;
}

// template<typename Iterator>
// [[nodiscard]] auto is_fully_initialized(Iterator first, const Iterator last, const std::remove_pointer_t<typename std::iterator_traits<Iterator>::value_type>* placeholder) noexcept -> bool {
//   for (; (first != last) && *first != placeholder; ++first) { }
//   return first == last;
// }

template<typename Result, typename View, typename Other, std::size_t... GetLhs, std::size_t... GetRhs>
[[nodiscard]] auto view_pack(const View& view, const Other& other, std::index_sequence<GetLhs...>, std::index_sequence<GetRhs...>) -> Result{
  auto element = Result{};

  element.pools = {view.template storage<GetLhs>()..., other.template storage<GetRhs>()...};

  // auto filter_or_placeholder = [placeholder = element.placeholder](auto* value) { return (value == nullptr) ? placeholder : value; };

  // element.filter = {filter_or_placeholder(view.template storage<sizeof...(GLhs)>())..., filter_or_placeholder(other.template storage<sizeof...(GRhs)>())...};
  element.refresh();

  return element;
}

template<typename Type, bool IsChecked, std::size_t Get, std::size_t Exclude>
class view_iterator final {

  template<typename, typename...>
  friend class extended_view_iterator;

  template<typename LhsType, auto... LhsArgs, typename RhsType, auto... RhsArgs>
  friend constexpr bool operator==(const view_iterator<LhsType, LhsArgs...>&, const view_iterator<RhsType, RhsArgs...>&) noexcept;

  using iterator_type = typename Type::const_iterator;
  using iterator_traits = std::iterator_traits<iterator_type>;

public:

  using common_type = Type;
  using value_type = typename iterator_traits::value_type;
  using pointer = typename iterator_traits::pointer;
  using reference = typename iterator_traits::reference;
  using difference_type = typename iterator_traits::difference_type;
  using iterator_category = std::forward_iterator_tag;

  constexpr view_iterator() noexcept
  : _iterator{},
    _pools{},
    _index{} { }

  view_iterator(iterator_type first, std::array<const common_type*, Get> value, std::array<const common_type*, Exclude> filter, const std::size_t _index) noexcept
  : _iterator{first},
    _pools{value},
    _filter{filter},
    _index{static_cast<difference_type>(_index)} {
    utility::assert_that((Get != 1u) || (Exclude != 0u) || _pools[0u]->policy() == deletion_policy::in_place, "Non in-place storage view iterator");
    _seek_next();
  }

  auto operator++() noexcept -> view_iterator& {
    ++_iterator;
    _seek_next();
    return *this;
  }

  auto operator++(int) noexcept -> view_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  [[nodiscard]] auto operator->() const noexcept -> pointer {
    return &*_iterator;
  }

  [[nodiscard]] auto operator*() const noexcept -> reference {
    return *operator->();
  }

private:

  [[nodiscard]] auto _is_valid(const value_type entity) const noexcept -> bool {
    return (!IsChecked || (entity != tombstone_entity)) 
        && ((Get == 1u) || (detail::all_of(_pools.begin(), _pools.begin() + _index, entity) && detail::all_of(_pools.begin() + _index + 1, _pools.end(), entity)))
        && ((Exclude == 0u) || detail::none_of(_filter.begin(), _filter.end(), entity));
  }

  auto _seek_next() -> void {
    for (constexpr auto sentinel = iterator_type{}; _iterator != sentinel && !_is_valid(*_iterator); ++_iterator) { }
  }

  iterator_type _iterator;
  std::array<const common_type*, Get> _pools;
  std::array<const common_type*, Exclude> _filter;
  difference_type _index;

}; // class view_iterator

template<typename LhsType, auto... LhsArgs, typename RhsType, auto... RhsArgs>
[[nodiscard]] constexpr auto operator==(const view_iterator<LhsType, LhsArgs...>& lhs, const view_iterator<RhsType, RhsArgs...>& rhs) noexcept -> bool {
  return lhs._iterator == rhs._iterator;
}

template<typename Iterator, typename... Get>
class extended_view_iterator final {

  template<typename... Lhs, typename... Rhs>
  friend auto constexpr operator==(const extended_view_iterator<Lhs...>&, const extended_view_iterator<Rhs...>&) noexcept -> bool;

public:

  using iterator_type = Iterator;
  using value_type = decltype(std::tuple_cat(std::make_tuple(*std::declval<Iterator>()), std::declval<Get>().get_as_tuple(std::declval<typename Get::entity_type>())...));
  using pointer = memory::input_iterator_pointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;

  constexpr extended_view_iterator()
  : _iterator{} { }

  extended_view_iterator(iterator_type from)
  : _iterator{from} { }

  auto operator++() noexcept -> extended_view_iterator& {
    return ++_iterator, *this;
  }

  auto operator++(int) noexcept -> extended_view_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  [[nodiscard]] auto operator*() const noexcept -> reference{
    return _dereference(std::index_sequence_for<Get...>{});
  }

  [[nodiscard]] auto operator->() const noexcept -> pointer {
    return operator*();
  }

  [[nodiscard]] constexpr auto base() const noexcept -> iterator_type {
    return _iterator;
  }

private:

  template<std::size_t... Index>
  [[nodiscard]] auto _dereference(std::index_sequence<Index...>) const noexcept {
    return std::tuple_cat(std::make_tuple(*_iterator), static_cast<Get*>(const_cast<utility::constness_as_t<typename Get::base_type, Get>*>(std::get<Index>(_iterator._pools)))->get_as_tuple(*_iterator)...);
  }

  Iterator _iterator;

}; // class extended_view_iterator

template<typename... Lhs, typename... Rhs>
[[nodiscard]] constexpr auto operator==(const extended_view_iterator<Lhs...>& lhs, const extended_view_iterator<Rhs...>& rhs) noexcept -> bool {
  return lhs._iterator == rhs._iterator;
}

} // namespace sbx::ecs::detail

#endif // LIBSBX_ECS_DETAIL_VIEW_ITERATOR_HPP_
