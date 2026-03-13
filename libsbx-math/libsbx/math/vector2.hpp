// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_VECTOR2_HPP_
#define LIBSBX_MATH_VECTOR2_HPP_

#include <cstddef>
#include <cinttypes>
#include <cmath>
#include <concepts>
#include <fstream>
#include <ostream>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector.hpp>

namespace sbx::math {

/**
 * @brief A vector in two-dimensional space.
 *
 * @tparam Type The type of the vectors components.
 */
template<scalar Type>
class basic_vector2 : public basic_vector<2u, Type> {

  using base_type = basic_vector<2u, Type>;

  inline static constexpr auto x_axis = std::size_t{0u};
  inline static constexpr auto y_axis = std::size_t{1u};

public:

  using value_type = base_type::value_type;
  using reference = base_type::reference;
  using const_reference = base_type::const_reference;
  using size_type = base_type::size_type;
  using length_type = base_type::length_type;

  inline static constexpr basic_vector2 zero{base_type::fill(value_type{0})};
  inline static constexpr basic_vector2 one{base_type::fill(value_type{1})};

  using base_type::base_type;

  constexpr basic_vector2(const base_type& base) noexcept;

  template<scalar Other>
  constexpr basic_vector2(Other x, Other y) noexcept;

  template<scalar Other>
  constexpr basic_vector2(Other value) noexcept;

  [[nodiscard]] static constexpr auto dot(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> length_type;

  [[nodiscard]] static constexpr auto normalized(const basic_vector2& vector) noexcept -> basic_vector2;

  [[nodiscard]] static constexpr auto orthogonal(const basic_vector2& vector) noexcept -> basic_vector2;

  [[nodiscard]] static constexpr auto determinant(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> value_type;

  [[nodiscard]] static constexpr auto distance(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> value_type;

  [[nodiscard]] constexpr auto x() noexcept -> reference;

  [[nodiscard]] constexpr auto x() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto y() noexcept -> reference;

  [[nodiscard]] constexpr auto y() const noexcept -> const_reference;

}; // class basic_vector2

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs>;

template<scalar Type>
[[nodiscard]] constexpr auto operator-(basic_vector2<Type> vector) noexcept -> basic_vector2<Type>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector2<Lhs> lhs, Rhs scalar) noexcept -> basic_vector2<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(Lhs scalar, basic_vector2<Rhs> rhs) noexcept -> basic_vector2<Rhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator/(basic_vector2<Lhs> lhs, Rhs scalar) noexcept -> basic_vector2<Lhs>;

using vector2f = basic_vector2<std::float_t>;

using vector2i = basic_vector2<std::int32_t>;

using vector2u = basic_vector2<std::uint32_t>;

using vector2 = vector2f;

} // namespace ::math

template<sbx::math::scalar Type>
struct std::hash<sbx::math::basic_vector2<Type>> {

  auto operator()(const sbx::math::basic_vector2<Type>& vector) const noexcept -> std::size_t;

}; // struct std::hash

template<sbx::math::scalar Type>
struct YAML::convert<sbx::math::basic_vector2<Type>> {

  static auto encode(const sbx::math::basic_vector2<Type>& vector) -> Node;

  static auto decode(const Node& node, sbx::math::basic_vector2<Type>& vector) -> bool;

}; // struct YAML::convert

template<sbx::math::scalar Type>
struct fmt::formatter<sbx::math::basic_vector2<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin());

  template<typename FormatContext>
  auto format(const sbx::math::basic_vector2<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out());

}; // struct fmt::formatter

template<sbx::math::scalar Type>
auto operator<<(YAML::Emitter& out, const sbx::math::basic_vector2<Type>& vector) -> YAML::Emitter& {
  return out << YAML::convert<sbx::math::basic_vector2<Type>>::encode(vector);
}

#include <libsbx/math/vector2.ipp>

#endif // LIBSBX_MATH_VECTOR2_HPP_
