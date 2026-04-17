// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_VECTOR4_HPP_
#define LIBSBX_MATH_VECTOR4_HPP_

#include <cstddef>
#include <cmath>
#include <cstdint>
#include <concepts>
#include <fstream>
#include <ostream>
#include <type_traits>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::math {

template<scalar Type>
class basic_vector4 : public basic_vector<4u, Type> {

  using base_type = basic_vector<4u, Type>;

  inline static constexpr auto x_axis = std::size_t{0u};
  inline static constexpr auto y_axis = std::size_t{1u};
  inline static constexpr auto z_axis = std::size_t{2u};
  inline static constexpr auto w_axis = std::size_t{3u};

public:

  using value_type = base_type::value_type;
  using reference = base_type::reference;
  using const_reference = base_type::const_reference;
  using size_type = base_type::size_type;
  using length_type = base_type::length_type;

  inline static constexpr basic_vector4 zero{base_type::fill(value_type{0})};
  inline static constexpr basic_vector4 one{base_type::fill(value_type{1})};

  using base_type::base_type;

  constexpr basic_vector4(const base_type& base) noexcept;

  template<scalar XOther, scalar YOther, scalar ZOther, scalar WOther>
  constexpr basic_vector4(XOther x, YOther y, ZOther z, WOther w) noexcept;

  template<scalar Other, scalar ScalarW = Other>
  constexpr basic_vector4(const basic_vector3<Other>& vector, ScalarW w = ScalarW{0}) noexcept;

  template<scalar Other, scalar ScalarZ = Other, scalar ScalarW = Other>
  constexpr basic_vector4(const basic_vector2<Other>& vector, ScalarZ z = ScalarZ{0}, ScalarW w = ScalarW{0}) noexcept;

  [[nodiscard]] static constexpr auto dot(const basic_vector4& lhs, const basic_vector4& rhs) noexcept -> length_type;

  [[nodiscard]] static constexpr auto normalized(const basic_vector4& vector) noexcept -> basic_vector4;

  [[nodiscard]] constexpr operator basic_vector3<Type>() const noexcept;

  [[nodiscard]] constexpr auto x() noexcept -> reference;

  [[nodiscard]] constexpr auto x() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto y() noexcept -> reference;

  [[nodiscard]] constexpr auto y() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto z() noexcept -> reference;

  [[nodiscard]] constexpr auto z() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto w() noexcept -> reference;

  [[nodiscard]] constexpr auto w() const noexcept -> const_reference;

}; // class basic_vector4

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs>;

template<scalar Type>
[[nodiscard]] constexpr auto operator-(basic_vector4<Type> vector) noexcept -> basic_vector4<Type>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector4<Lhs> lhs, Rhs scalar) noexcept -> basic_vector4<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator/(basic_vector4<Lhs> lhs, Rhs scalar) noexcept -> basic_vector4<Lhs>;

using vector4f = basic_vector4<std::float_t>;

using vector4u = basic_vector4<std::uint32_t>;

using vector4i = basic_vector4<std::int32_t>;

using vector4 = vector4f;

} // namespace sbx::math

template<sbx::math::scalar Type>
struct std::hash<sbx::math::basic_vector4<Type>> {

  auto operator()(const sbx::math::basic_vector4<Type>& vector) const noexcept -> std::size_t;

}; // struct std::hash

template<sbx::math::scalar Type>
struct fmt::formatter<sbx::math::basic_vector4<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) noexcept -> decltype(context.begin());

  template<typename FormatContext>
  auto format(const sbx::math::basic_vector4<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out());

}; // struct fmt::formatter

template<sbx::math::scalar Type>
struct YAML::convert<sbx::math::basic_vector4<Type>> {

  static auto encode(const sbx::math::basic_vector4<Type>& vector) -> Node;

  static auto decode(const Node& node, sbx::math::basic_vector4<Type>& vector) -> bool;

}; // struct YAML::convert

#include <libsbx/math/vector4.ipp>

#endif // LIBSBX_MATH_VECTOR4_HPP_
