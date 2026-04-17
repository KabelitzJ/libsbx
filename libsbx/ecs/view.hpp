// SPDX-License-Identifier: MIT
#ifndef LIBSBX_VIEW_HPP_
#define LIBSBX_VIEW_HPP_

#include <algorithm>
#include <tuple>
#include <ranges>
#include <vector>
#include <array>

#include <libsbx/utility/type_list.hpp>

#include <libsbx/memory/concepts.hpp>
#include <libsbx/memory/iterable_adaptor.hpp>

#include <libsbx/ecs/detail/view_iterator.hpp>

namespace sbx::ecs {

namespace detail {

template<typename Type, bool IsChecked, std::size_t Get, std::size_t Exclude>
requires (std::is_same_v<std::remove_const_t<std::remove_reference_t<Type>>, Type>)
class basic_common_view {
  
  template<typename Return, typename View, typename Other, std::size_t... GetLhs, std::size_t... GetRhs>
  friend auto view_pack(const View&, const Other&, std::index_sequence<GetLhs...>, std::index_sequence<GetRhs...>) -> Return;

public:

  using common_type = Type;
  using entity_type = typename Type::entity_type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = detail::view_iterator<common_type, IsChecked, Get, Exclude>;

  auto refresh() noexcept -> void {
    auto position = static_cast<size_type>(_index != Get) * Get;

    for (; position < Get && _pools[position] != nullptr; ++position) { }

    if (position == Get) {
      _unchecked_refresh();
    }
  }

  [[nodiscard]] auto handle() const noexcept -> const common_type* {
    return (_index != Get) ? _pools[_index] : nullptr;
  }

  [[nodiscard]] auto size_hint() const noexcept -> size_type {
    return (_index != Get) ? _offset() : size_type{};
  }

  [[nodiscard]] auto begin() const noexcept -> iterator {
    return (_index != Get) ? iterator{_pools[_index]->end() - static_cast<difference_type>(_offset()), _pools, _filter, _index} : iterator{};
  }

  [[nodiscard]] auto end() const noexcept -> iterator {
    return (_index != Get) ? iterator{_pools[_index]->end(), _pools, _filter, _index} : iterator{};
  }

  [[nodiscard]] auto front() const noexcept -> entity_type {
    const auto it = begin();
    return it != end() ? *it : null_entity;
  }

  [[nodiscard]] auto back() const noexcept -> entity_type {
    if(_index != Get) {
      auto it = _pools[_index]->rbegin();
      const auto last = it + static_cast<difference_type>(_offset());

      for (const auto idx = static_cast<difference_type>(_index); it != last && !(detail::all_of(_pools.begin(), _pools.begin() + idx, *it) && detail::all_of(_pools.begin() + idx + 1, _pools.end(), *it)); ++it) { }

      return it == last ? null_entity : *it;
    }

    return null_entity;
  }

  [[nodiscard]] auto find(const entity_type entity) const noexcept -> iterator {
    return contains(entity) ? iterator{_pools[_index]->find(entity), _pools, _index} : end();
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return (_index != Get); // && detail::is_fully_initialized(filter.begin(), filter.end(), placeholder);
  }

  [[nodiscard]] auto contains(const entity_type entity) const noexcept -> bool {
    return (_index != Get) && detail::all_of(_pools.begin(), _pools.end(), entity) && _pools[_index]->_index(entity) < _offset();
  }

protected:

  basic_common_view() noexcept
  : _pools{},
    _index{Get} { }

  basic_common_view(std::array<const common_type*, Get> pools, std::array<const common_type*, Exclude> filter) noexcept
  : _pools{pools},
    _filter{filter},
    _index{Get} {
    _unchecked_refresh();
  }

  [[nodiscard]] auto pool_at(const size_type position) const noexcept -> const common_type* {
    return _pools[position];
  }

  auto set_pool_at(const size_type position, const common_type* element) noexcept -> void {
    utility::assert_that(element != nullptr, "Unexpected element");
    _pools[position] = element;
    refresh();
  }

  auto use(const size_type position) noexcept -> void {
    _index = (_index != Get) ? position : Get;
  }

private:

  [[nodiscard]] auto _offset() const noexcept {
    utility::assert_that(_index != Get, "Invalid view");
    return (_pools[_index]->policy() == deletion_policy::swap_only) ? _pools[_index]->free_list() : _pools[_index]->size();
  }

  auto _unchecked_refresh() noexcept -> void {
    _index = 0u;

    if constexpr (Get > 1u) {
      for (auto position  = 1u; position < Get; ++position) {
        if (_pools[position]->size() < _pools[_index]->size()) {
          _index = position;
        }
      }
    }
  }

  std::array<const common_type*, Get> _pools;
  std::array<const common_type*, Exclude> _filter;
  size_type _index;

}; // class basic_common_view

template<typename Type, deletion_policy Policy>
requires (std::is_same_v<std::remove_const_t<std::remove_reference_t<Type>>, Type>)
class basic_storage_view {

public:

  using common_type = Type;
  using entity_type = typename common_type::entity_type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = std::conditional_t<Policy == deletion_policy::in_place, detail::view_iterator<common_type, true, 1u, 0u>, typename common_type::iterator>;
  using reverse_iterator = std::conditional_t<Policy == deletion_policy::in_place, void, typename common_type::reverse_iterator>;

  [[nodiscard]] auto handle() const noexcept -> const common_type* {
    return _leading;
  }

  template<typename..., deletion_policy Pol = Policy>
  requires (Pol != deletion_policy::in_place)
  [[nodiscard]] auto size() const noexcept -> size_type {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return _leading ? _leading->size() : size_type{};
    } else {
      static_assert(Policy == deletion_policy::swap_only, "Unexpected storage policy");
      return _leading ? _leading->free_list() : size_type{};
    }
  }

  template<typename..., deletion_policy Pol = Policy>
  requires (Pol == deletion_policy::in_place)
  [[nodiscard]] auto size_hint() const noexcept -> size_type {
      return _leading ? _leading->size() : size_type{};
  }

  template<typename..., deletion_policy Pol = Policy>
  requires (Pol != deletion_policy::in_place)
  [[nodiscard]] auto is_empty() const noexcept -> bool {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return !_leading || _leading->is_empty();
    } else {
      static_assert(Policy == deletion_policy::swap_only, "Unexpected storage policy");
      return !_leading || (_leading->free_list() == 0u);
    }
  }

  [[nodiscard]] auto begin() const noexcept -> iterator {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return _leading ? _leading->begin() : iterator{};
    } else if constexpr (Policy == deletion_policy::swap_only) {
      return _leading ? (_leading->end() - static_cast<difference_type>(_leading->free_list())) : iterator{};
    } else {
      static_assert(Policy == deletion_policy::in_place, "Unexpected storage policy");
      return _leading ? iterator{_leading->begin(), {_leading}, {}, 0u} : iterator{};
    }
  }

  [[nodiscard]] auto end() const noexcept -> iterator {
    if constexpr (Policy == deletion_policy::swap_and_pop || Policy == deletion_policy::swap_only) {
      return _leading ? _leading->end() : iterator{};
    } else {
      static_assert(Policy == deletion_policy::in_place, "Unexpected storage policy");
      return _leading ? iterator{_leading->end(), {_leading}, {}, 0u} : iterator{};
    }
  }

  template<typename..., deletion_policy Pol = Policy>
  requires (Pol != deletion_policy::in_place)
  [[nodiscard]] auto rbegin() const noexcept -> reverse_iterator {
    return _leading ? _leading->rbegin() : reverse_iterator{};
  }

  template<typename..., deletion_policy Pol = Policy>
  requires (Pol != deletion_policy::in_place)
  [[nodiscard]] auto rend() const noexcept -> reverse_iterator {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return _leading ? _leading->rend() : reverse_iterator{};
    } else {
      static_assert(Policy == deletion_policy::swap_only, "Unexpected storage policy");
      return _leading ? (_leading->rbegin() + static_cast<difference_type>(_leading->free_list())) : reverse_iterator{};
    }
  }

  [[nodiscard]] auto front() const noexcept -> entity_type {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return is_empty() ? null_entity : *_leading->begin();
    } else if constexpr (Policy == deletion_policy::swap_only) {
      return is_empty() ? null_entity : *(_leading->end() - static_cast<difference_type>(_leading->free_list()));
    } else {
      static_assert(Policy == deletion_policy::in_place, "Unexpected storage policy");
      const auto it = begin();

      return (it == end()) ? null_entity : *it;
    }
  }

  [[nodiscard]] auto back() const noexcept -> entity_type {
    if constexpr (Policy == deletion_policy::swap_and_pop || Policy == deletion_policy::swap_only) {
      return is_empty() ? null_entity : *_leading->rbegin();
    } else {
      static_assert(Policy == deletion_policy::in_place, "Unexpected storage policy");

      if (_leading) {
        auto it = _leading->rbegin();
        const auto last = _leading->rend();

        for(; (it != last) && (*it == tombstone_entity); ++it) { }
        
        return it == last ? null_entity : *it;
      }

      return null_entity;
    }
  }

  [[nodiscard]] auto find(const entity_type entity) const noexcept -> iterator {
    if constexpr (Policy == deletion_policy::swap_and_pop) {
      return _leading ? _leading->find(entity) : iterator{};
    } else if constexpr (Policy == deletion_policy::swap_only) {
      const auto it = _leading ? _leading->find(entity) : iterator{};
      return _leading && (static_cast<size_type>(it.index()) < _leading->free_list()) ? it : iterator{};
    } else {
      return _leading ? iterator{_leading->find(entity), {_leading}, {}, 0u} : iterator{};
    }
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return (_leading != nullptr);
  }

  [[nodiscard]] auto contains(const entity_type entity) const noexcept -> bool {
    if constexpr (Policy == deletion_policy::swap_and_pop || Policy == deletion_policy::in_place) {
      return _leading && _leading->contains(entity);
    } else {
      static_assert(Policy == deletion_policy::swap_only, "Unexpected storage policy");
      return _leading && _leading->contains(entity) && (_leading->index(entity) < _leading->free_list());
    }
  }

protected:

  basic_storage_view() noexcept = default;

  basic_storage_view(const common_type* value) noexcept
  : _leading{value} {
    utility::assert_that(_leading->policy() == Policy, "Unexpected storage policy");
  }

private:

  const common_type* _leading;

}; // class basic_storage_view


} // namespace detail

template<typename... Type>
struct get_t final : utility::type_list<Type...> {
  explicit constexpr get_t() = default;
}; // struct get_t

template<typename... Type>
inline constexpr get_t<Type...> get{};

template<typename... Type>
struct exclude_t final : utility::type_list<Type...> {
  explicit constexpr exclude_t() = default;
}; // struct exclude

template<typename... Type>
inline constexpr exclude_t<Type...> exclude{};

template<typename, typename>
class basic_view;

template<typename... Get, typename... Exclude>
requires (sizeof...(Get) != 0u)
class basic_view<get_t<Get...>, exclude_t<Exclude...>> : public detail::basic_common_view<std::common_type_t<typename Get::base_type...>, detail::tombstone_check_v<Get...>, sizeof...(Get), sizeof...(Exclude)> {

  using base_type = detail::basic_common_view<std::common_type_t<typename Get::base_type...>, detail::tombstone_check_v<Get...>, sizeof...(Get), sizeof...(Exclude)>;

  template<std::size_t Index>
  using element_at = utility::type_list_element_t<Index, utility::type_list<Get..., Exclude...>>;

  template<typename Type>
  inline static constexpr auto index_of = utility::type_list_index_v<std::remove_const_t<Type>, utility::type_list<typename Get::element_type..., typename Exclude::element_type...>>;

public:

  using common_type = typename base_type::common_type;
  using entity_type = typename base_type::entity_type;
  using size_type = typename base_type::size_type;
  using difference_type = std::ptrdiff_t;
  using iterator = typename base_type::iterator;
  using iterable = memory::iterable_adaptor<detail::extended_view_iterator<iterator, Get...>>;

  basic_view() noexcept
  : base_type{} { }


  basic_view(Get&... pools, Exclude&... filter) noexcept
  : base_type{{&pools...}, {&filter...}} { }

  basic_view(std::tuple<Get&...> pools, std::tuple<Exclude&...> filter = {}) noexcept
  : basic_view{std::make_from_tuple<basic_view>(std::tuple_cat(pools, filter))} { }

  template<typename Type>
  [[nodiscard]] auto* storage() const noexcept {
    return storage<index_of<Type>>();
  }

  template<std::size_t Index>
  [[nodiscard]] auto* storage() const noexcept {
    return static_cast<element_at<Index>*>(const_cast<utility::constness_as_t<common_type, element_at<Index>>*>(base_type::pool_at(Index)));
  }

  template<typename Type>
  auto set_storage(Type& element) noexcept -> void {
    set_storage<index_of<typename Type::element_type>>(element);
  }

  template<std::size_t Index, typename Type>
  requires (std::is_convertible_v<Type&, element_at<Index>&>)
  void set_storage(Type& element) noexcept {
    base_type::set_pool_at(Index, &element);
  }

  template<typename Type, typename... Other>
  [[nodiscard]] auto get(const entity_type entity) const -> decltype(auto) {
    return get<index_of<Type>, index_of<Other>...>(entity);
  }

  template<std::size_t... Index>
  [[nodiscard]] auto get(const entity_type entity) const -> decltype(auto) {
    if constexpr(sizeof...(Index) == 0) {
      return _get(entity, std::index_sequence_for<Get...>{});
    } else if constexpr(sizeof...(Index) == 1) {
      return (storage<Index>()->get(entity), ...);
    } else {
      return std::tuple_cat(storage<Index>()->get_as_tuple(entity)...);
    }
  }
  
  [[nodiscard]] auto each() const noexcept -> iterable {
    return {base_type::begin(), base_type::end()};
  }

private:

  template<std::size_t... Index>
  [[nodiscard]] auto _get(const entity_type entity, std::index_sequence<Index...>) const noexcept {
    return std::tuple_cat(storage<Index>()->get_as_tuple(entity)...);
  }

}; // class basic_view

template<typename Get>
class basic_view<get_t<Get>, exclude_t<>> : public detail::basic_storage_view<typename Get::base_type, Get::storage_policy> {

  using base_type = detail::basic_storage_view<typename Get::base_type, Get::storage_policy>;

public:

  using common_type = typename base_type::common_type;
  using entity_type = typename base_type::entity_type;
  using size_type = typename base_type::size_type;
  using difference_type = std::ptrdiff_t;
  using iterator = typename base_type::iterator;
  using reverse_iterator = typename base_type::reverse_iterator;
  using iterable = std::conditional_t<Get::storage_policy == deletion_policy::in_place, memory::iterable_adaptor<detail::extended_view_iterator<iterator, Get>>, decltype(std::declval<Get>().each())>;

  basic_view() noexcept
  : base_type{} { }

  basic_view(Get& value) noexcept
  : base_type{&value} { }

  basic_view(std::tuple<Get&> value) noexcept
  : basic_view{std::get<0>(value)} {}

  template<typename Type = typename Get::element_type>
  requires (std::is_same_v<std::remove_const_t<Type>, typename Get::element_type>)
  [[nodiscard]] auto* storage() const noexcept {
    return storage<0>();
  }

  template<std::size_t Index>
  requires (Index == 0u)
  [[nodiscard]] auto* storage() const noexcept {
    return static_cast<Get*>(const_cast<utility::constness_as_t<common_type, Get>*>(base_type::handle()));
  }

  void set_storage(Get& element) noexcept {
    set_storage<0>(element);
  }

  template<std::size_t Index>
  requires (Index == 0u)
  void set_storage(Get& element) noexcept {
    *this = basic_view{element};
  }

  [[nodiscard]] auto operator->() const noexcept -> Get* {
    return storage();
  }

  [[nodiscard]] auto operator[](const entity_type entity) const -> decltype(auto) {
    return storage()->get(entity);
  }

  template<typename Element>
  requires (std::is_same_v<std::remove_const_t<Element>, typename Get::element_type>)
  [[nodiscard]] auto get(const entity_type entity) const -> decltype(auto) {
    return get<0>(entity);
  }

  template<std::size_t... Index>
  [[nodiscard]] auto get(const entity_type entity) const -> decltype(auto) {
    if constexpr(sizeof...(Index) == 0) {
      return storage()->get_as_tuple(entity);
    } else {
      return storage<Index...>()->get(entity);
    }
  }

  [[nodiscard]] auto each() const noexcept -> iterable {
    if constexpr(Get::storage_policy == deletion_policy::swap_and_pop || Get::storage_policy == deletion_policy::swap_only) {
      return base_type::handle() ? storage()->each() : iterable{};
    } else {
      static_assert(Get::storage_policy == deletion_policy::in_place, "Unexpected storage policy");
      return iterable{base_type::begin(), base_type::end()};
    }
  }

}; // class basic_view

template<typename... Type>
basic_view(Type&...storage) -> basic_view<get_t<Type...>, exclude_t<>>;

template<typename... Get, typename... Exclude>
basic_view(std::tuple<Get&...>, std::tuple<Exclude&...> = {}) -> basic_view<get_t<Get...>, exclude_t<Exclude...>>;

} // namespace sbx::ecs

#endif // LIBSBX_VIEW_HPP_
