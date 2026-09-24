// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/containers/compressed_pair.hpp
 *
 * @brief A compressed pair implementation using the empty base optimization.
 *
 * @ingroup libsbx-containers
 */

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

template<typename Type>
concept ebco_eligible = is_ebco_eligible_v<Type>;

/**
 * @brief Holds one element of a compressed_pair. Non-EBO-eligible primary template: stores
 * Type as a plain member.
 *
 * @tparam Type The element type.
 * @tparam (unnamed) Distinguishes the two elements of a pair of the same Type — see the
 * compressed_pair_element<ebco_eligible Type, Tag> specialization for why this matters.
 */
template<typename Type, std::size_t>
class compressed_pair_element {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;

  constexpr compressed_pair_element() noexcept(std::is_nothrow_default_constructible_v<value_type>) requires (std::is_default_constructible_v<value_type>)
  : _value{} { }

  template<typename Arg>
  requires (!std::same_as<std::remove_cvref_t<Arg>, compressed_pair_element>)
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

/**
 * @brief Holds one element of a compressed_pair. EBO-eligible specialization: Type is empty (and non-final), so compressed_pair_element inherits from it instead of storing it as a member, letting the empty base optimization eliminate its storage entirely.
 *
 * @tparam Type The (empty) element type.
 * @tparam Tag Distinguishes the two elements of a pair of the same Type; without it, compressed_pair could not privately inherit from Type twice (an unambiguous base requires a distinct type per base, and Tag makes this specialization's Type/Tag combination unique per pair slot).
 */
template<ebco_eligible Type, std::size_t Tag>
class compressed_pair_element<Type, Tag> : public Type {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using base_type = Type;

  constexpr compressed_pair_element() noexcept(std::is_nothrow_default_constructible_v<base_type>) requires (std::is_default_constructible_v<base_type>)
  : base_type{} { }

  template<typename Arg>
  requires (!std::same_as<std::remove_cvref_t<Arg>, compressed_pair_element>)
  constexpr compressed_pair_element(Arg&& arg) noexcept(std::is_nothrow_constructible_v<base_type, Arg>)
  : base_type{std::forward<Arg>(arg)} { }

  template<typename... Args, std::size_t... Index>
  constexpr compressed_pair_element(std::tuple<Args...> args, std::index_sequence<Index...>) noexcept(std::is_nothrow_constructible_v<base_type, Args...>)
  : base_type{std::forward<Args>(std::get<Index>(args))...} { }

  [[nodiscard]] constexpr auto get() noexcept -> reference {
    return *this;
  }

  [[nodiscard]] constexpr auto get() const noexcept -> const_reference {
    return *this;
  }

}; // struct compressed_pair_element

} // namespace detail

/**
 * @brief A pair of two values, like std::pair, except an empty (and non-final) First/Second is stored via the empty base optimization instead of taking up space, so e.g. a pair of a value and a stateless comparator/hasher costs nothing extra over the value alone.
 *
 * @tparam First The first element's type.
 * @tparam Second The second element's type.
 */
template<typename First, typename Second>
class compressed_pair final : detail::compressed_pair_element<First, 0u>, detail::compressed_pair_element<Second, 1u> {

  using first_base = detail::compressed_pair_element<First, 0u>;
  using second_base = detail::compressed_pair_element<Second, 1u>;

public:

  using first_type = First;
  using second_type = Second;

  constexpr compressed_pair() noexcept(std::is_nothrow_default_constructible_v<first_base> && std::is_nothrow_default_constructible_v<second_base>) requires (std::is_default_constructible_v<first_type> && std::is_default_constructible_v<second_type>)
  : first_base{},
    second_base{} { }

  constexpr compressed_pair(const compressed_pair& other) = default;

  constexpr compressed_pair(compressed_pair&& other) noexcept = default;

  /**
   * @brief Constructs first and second directly from arg/other.
   *
   * @tparam Arg The type to construct first from.
   * @tparam Other The type to construct second from.
   *
   * @param arg The value to construct first from.
   * @param other The value to construct second from.
   */
  template<typename Arg, typename Other>
  constexpr compressed_pair(Arg&& arg, Other&& other) noexcept(std::is_nothrow_constructible_v<first_base, Arg> && std::is_nothrow_constructible_v<second_base, Other>)
  : first_base{std::forward<Arg>(arg)},
    second_base{std::forward<Other>(other)} { }

  /**
   * @brief Constructs first and second in place, each from its own argument tuple --
   * for element types with no single-argument constructor, or to avoid an intermediate copy/move.
   *
   * @tparam Args The types of first's constructor arguments.
   * @tparam Other The types of second's constructor arguments.
   *
   * @param args Forwarded to first_type's constructor.
   * @param other Forwarded to second_type's constructor.
   */
  template<typename... Args, typename... Other>
  constexpr compressed_pair(std::piecewise_construct_t, std::tuple<Args...> args, std::tuple<Other...> other) noexcept(std::is_nothrow_constructible_v<first_base, Args...> && std::is_nothrow_constructible_v<second_base, Other...>)
  : first_base{std::move(args), std::index_sequence_for<Args...>{}},
    second_base{std::move(other), std::index_sequence_for<Other...>{}} { }

  ~compressed_pair() = default;

  constexpr compressed_pair& operator=(const compressed_pair& other) = default;

  constexpr compressed_pair& operator=(compressed_pair&& other) noexcept = default;

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

  constexpr auto swap(compressed_pair& other) noexcept -> void {
    using std::swap;
    swap(first(), other.first());
    swap(second(), other.second());
  }

  /** @return first() if Index == 0, else second(). For structured bindings (`auto& [a, b] = pair;`). */
  template<std::size_t Index>
  requires (Index < 2u)
  [[nodiscard]] constexpr auto get() noexcept -> decltype(auto) {
    if constexpr (Index == 0u) {
      return first();
    } else {
      return second();
    }
  }

  /** @copydoc get */
  template<std::size_t Index>
  requires (Index < 2u)
  [[nodiscard]] constexpr auto get() const noexcept -> decltype(auto) {
    if constexpr (Index == 0u) {
      return first();
    } else {
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
requires (Index < 2u)
struct std::tuple_element<Index, sbx::containers::compressed_pair<First, Second>> : std::conditional<Index == 0u, First, Second> { };

#endif // LIBSBX_CONTAINERS_COMPRESSED_PAIR_HPP_
