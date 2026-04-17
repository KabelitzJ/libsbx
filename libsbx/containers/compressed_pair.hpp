// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_COMPRESSED_PAIR_HPP_
#define LIBSBX_CONTAINERS_COMPRESSED_PAIR_HPP_

#include <type_traits>
#include <utility>

namespace sbx::containers {

namespace detail {

template<typename Type>
struct is_ebco_eligible : std::bool_constant<std::is_empty_v<Type> && !std::is_final_v<Type>> { };

template<typename Type>
inline constexpr bool is_ebco_eligible_v = is_ebco_eligible<Type>::value;

template<typename Type, std::size_t, typename = void>
class compressed_pair_element {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;

  template<typename Dummy = value_type, typename = std::enable_if_t<std::is_default_constructible_v<Dummy>>>
  constexpr compressed_pair_element() noexcept(std::is_nothrow_default_constructible_v<value_type>) {}

  template<typename Arg, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Arg>>, compressed_pair_element>>>
  constexpr compressed_pair_element(Arg&& arg) noexcept(std::is_nothrow_constructible_v<value_type, Arg>)
  : _value{std::forward<Arg>(arg)} { }

  template<typename... Args, std::size_t... Index>
  constexpr compressed_pair_element(std::tuple<Args...> args, std::index_sequence<Index...>) noexcept(std::is_nothrow_constructible_v<value_type, Args...>)
  : _value{std::forward<Args>(std::get<Index>(args))...} { }

  [[nodiscard]] constexpr auto get() noexcept -> reference {
    return _value;
  }

  [[nodiscard]] constexpr auto get() const noexcept -> const_reference {
    return _value;
  }

private:

  value_type _value{};

}; // struct compressed_pair_element

template<typename Type, std::size_t Tag>
class compressed_pair_element<Type, Tag, std::enable_if_t<is_ebco_eligible_v<Type>>> : public Type {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using base_type = Type;

  template<typename Dummy = value_type, typename = std::enable_if_t<std::is_default_constructible_v<Dummy>>>
  constexpr compressed_pair_element() noexcept(std::is_nothrow_default_constructible_v<base_type>)
  : base_type{} { }

  template<typename Arg, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Arg>>, compressed_pair_element>>>
  constexpr compressed_pair_element(Arg&& arg) noexcept(std::is_nothrow_constructible_v<base_type, Arg>)
  : base_type{std::forward<Arg>(arg)} { }

  template<typename... Args, std::size_t... Index>
  constexpr compressed_pair_element(std::tuple<Args...> args, std::index_sequence<Index...>) noexcept(std::is_nothrow_constructible_v<base_type, Args...>)
  : base_type{std::forward<Args>(std::get<Index>(args))...} {}

  [[nodiscard]] constexpr auto get() noexcept -> reference {
    return *this;
  }

  [[nodiscard]] constexpr auto get() const noexcept -> const_reference {
    return *this;
  }

}; // struct compressed_pair_element

} // namespace detail

template<typename First, typename Second>
class compressed_pair final : detail::compressed_pair_element<First, 0u>, detail::compressed_pair_element<Second, 1u> {

  using first_base = detail::compressed_pair_element<First, 0u>;
  using second_base = detail::compressed_pair_element<Second, 1u>;

public:

  using first_type = First;
  using second_type = Second;

  template<bool Dummy = true, typename = std::enable_if_t<Dummy && std::is_default_constructible_v<first_type> && std::is_default_constructible_v<second_type>>>
  constexpr compressed_pair() noexcept(std::is_nothrow_default_constructible_v<first_base> && std::is_nothrow_default_constructible_v<second_base>)
  : first_base{},
    second_base{} { }

  constexpr compressed_pair(const compressed_pair &other) = default;

  constexpr compressed_pair(compressed_pair &&other) noexcept = default;

  template<typename Arg, typename Other>
  constexpr compressed_pair(Arg&& arg, Other&& other) noexcept(std::is_nothrow_constructible_v<first_base, Arg> && std::is_nothrow_constructible_v<second_base, Other>)
  : first_base{std::forward<Arg>(arg)},
    second_base{std::forward<Other>(other)} { }
  
  template<typename... Args, typename... Other>
  constexpr compressed_pair(std::piecewise_construct_t, std::tuple<Args...> args, std::tuple<Other...> other) noexcept(std::is_nothrow_constructible_v<first_base, Args...> && std::is_nothrow_constructible_v<second_base, Other...>)
  : first_base{std::move(args), std::index_sequence_for<Args...>{}},
    second_base{std::move(other), std::index_sequence_for<Other...>{}} { }

  ~compressed_pair() = default;

  constexpr compressed_pair &operator=(const compressed_pair &other) = default;

  constexpr compressed_pair &operator=(compressed_pair &&other) noexcept = default;

  [[nodiscard]] constexpr auto first() noexcept -> first_type& {
    return static_cast<first_base&>(*this).get();
  }

  [[nodiscard]] constexpr auto first() const noexcept -> const first_type& {
    return static_cast<const first_base&>(*this).get();
  }

  [[nodiscard]] constexpr auto second() noexcept -> second_type& {
    return static_cast<second_base&>(*this).get();
  }

  [[nodiscard]] constexpr auto second() const noexcept -> const second_type& {
    return static_cast<const second_base&>(*this).get();
  }

  constexpr void swap(compressed_pair &other) noexcept {
    using std::swap;
    swap(first(), other.first());
    swap(second(), other.second());
  }
  template<std::size_t Index>
  [[nodiscard]] constexpr auto get() noexcept -> decltype(auto) {
    if constexpr (Index == 0u) {
      return first();
    } else {
      static_assert(Index == 1u, "Index out of bounds");
      return second();
    }
  }

  /*! @copydoc get */
  template<std::size_t Index>
  [[nodiscard]] constexpr auto get() const noexcept -> decltype(auto) {
    if constexpr (Index == 0u) {
      return first();
    } else {
      static_assert(Index == 1u, "Index out of bounds");
      return second();
    }
  }

}; // class compressed_pair

template<typename Type, typename Other>
compressed_pair(Type&&, Other&&) -> compressed_pair<std::decay_t<Type>, std::decay_t<Other>>;

template<typename First, typename Second>
constexpr auto swap(compressed_pair<First, Second>& lhs, compressed_pair<First, Second>& rhs) noexcept -> void {
  lhs.swap(rhs);
}

} // namespace sbx::containers

template<typename First, typename Second>
struct std::tuple_size<sbx::containers::compressed_pair<First, Second>> : std::integral_constant<std::size_t, 2u> { };

template<std::size_t Index, typename First, typename Second>
struct std::tuple_element<Index, sbx::containers::compressed_pair<First, Second>> : std::conditional<Index == 0u, First, Second> {
  static_assert(Index < 2u, "Index out of bounds");
}; // struct std::tuple_element

#endif // LIBSBX_CONTAINERS_COMPRESSED_PAIR_HPP_
