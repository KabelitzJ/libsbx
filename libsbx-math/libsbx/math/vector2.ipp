// SPDX-License-Identifier: MIT
#include <libsbx/math/vector2.hpp>

#include <libsbx/utility/hash.hpp>

namespace sbx::math {

template<scalar Type>
inline constexpr basic_vector2<Type>::basic_vector2(const base_type& base) noexcept
: base_type{base} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_vector2<Type>::basic_vector2(Other x, Other y) noexcept
: base_type{x, y} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_vector2<Type>::basic_vector2(Other value) noexcept
: base_type{value, value} { }

template<scalar Type>
inline constexpr auto basic_vector2<Type>::dot(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> length_type {
  return lhs.x() * rhs.x() + lhs.y() * rhs.y();
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::normalized(const basic_vector2& vector) noexcept -> basic_vector2 {
  const auto length_squared = vector.length_squared();

  if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
    const auto length = std::sqrt(length_squared);
    return vector / length;
  }

  return vector;
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::orthogonal(const basic_vector2& vector) noexcept -> basic_vector2 {
  return basic_vector2{-vector.y(), vector.x()};
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::determinant(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> value_type {
  return lhs.x() * rhs.y() - lhs.y() * rhs.x();
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::distance(const basic_vector2& lhs, const basic_vector2& rhs) noexcept -> value_type {
  return (lhs - rhs).length();
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::x() noexcept -> reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::x() const noexcept -> const_reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::y() noexcept -> reference {
  return base_type::operator[](y_axis);
}

template<scalar Type>
inline constexpr auto basic_vector2<Type>::y() const noexcept -> const_reference {
  return base_type::operator[](y_axis);
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator+(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs> {
  return lhs += rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator-(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs> {
  return lhs -= rhs;
}

template<scalar Type>
inline constexpr auto operator-(basic_vector2<Type> vector) noexcept -> basic_vector2<Type> {
  return vector *= Type{-1};
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector2<Lhs> lhs, Rhs scalar) noexcept -> basic_vector2<Lhs> {
  return lhs *= scalar;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(Lhs scalar, basic_vector2<Rhs> rhs) noexcept -> basic_vector2<Rhs> {
  return rhs *= scalar;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector2<Lhs> lhs, const basic_vector2<Rhs>& rhs) noexcept -> basic_vector2<Lhs> {
  return lhs *= rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator/(basic_vector2<Lhs> lhs, Rhs scalar) noexcept -> basic_vector2<Lhs> {
  return lhs /= scalar;
}

} // namespace sbx::math

template<sbx::math::scalar Type>
inline auto std::hash<sbx::math::basic_vector2<Type>>::operator()(const sbx::math::basic_vector2<Type>& vector) const noexcept -> std::size_t {
  auto seed = std::size_t{0};

  sbx::utility::hash_combine(seed, vector.x(), vector.y());

  return seed;
}

template<sbx::math::scalar Type>
inline auto YAML::convert<sbx::math::basic_vector2<Type>>::encode(const sbx::math::basic_vector2<Type>& rhs) -> YAML::Node {
  auto node = Node{};

  node.SetStyle(YAML::EmitterStyle::Flow);

  node["x"] = rhs.x();
  node["y"] = rhs.y();

  return node;
}

template<sbx::math::scalar Type>
inline auto YAML::convert<sbx::math::basic_vector2<Type>>::decode(const YAML::Node& node, sbx::math::basic_vector2<Type>& rhs) -> bool {
  if (!node.IsMap()) {
    return false;
  }

  rhs.x() = node["x"].as<Type>();
  rhs.y() = node["y"].as<Type>();

  return true;
}

template<sbx::math::scalar Type>
template<typename ParseContext>
inline constexpr auto fmt::formatter<sbx::math::basic_vector2<Type>>::parse(ParseContext& context) -> decltype(context.begin()) {
  return context.begin();
}

template<sbx::math::scalar Type>
template<typename FormatContext>
inline auto fmt::formatter<sbx::math::basic_vector2<Type>>::format(const sbx::math::basic_vector2<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out()) {
  if constexpr (sbx::math::is_floating_point_v<Type>) {
    return fmt::format_to(context.out(), "{{x: {:.2f}, y: {:.2f}}}", vector.x(), vector.y());
  } else {
    return fmt::format_to(context.out(), "{{x: {}, y: {}}}", vector.x(), vector.y());
  }
}

