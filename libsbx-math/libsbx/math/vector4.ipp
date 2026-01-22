// SPDX-License-Identifier: MIT
#include <libsbx/math/vector4.hpp>

#include <cmath>

#include <libsbx/utility/hash.hpp>

namespace sbx::math {

template<scalar Type>
inline constexpr basic_vector4<Type>::basic_vector4(const base_type& base) noexcept
: base_type{base} { }

template<scalar Type>
template<scalar XOther, scalar YOther, scalar ZOther, scalar WOther>
inline constexpr basic_vector4<Type>::basic_vector4(XOther x, YOther y, ZOther z, WOther w) noexcept
: base_type{x, y, z, w} { }

template<scalar Type>
template<scalar Other, scalar ScalarW>
inline constexpr basic_vector4<Type>::basic_vector4(const basic_vector3<Other>& vector, ScalarW w) noexcept
: base_type{vector.x(), vector.y(), vector.z(), w} { }

template<scalar Type>
template<scalar Other, scalar ScalarZ, scalar ScalarW>
inline constexpr basic_vector4<Type>::basic_vector4(const basic_vector2<Other>& vector, ScalarZ z, ScalarW w) noexcept
: base_type{vector.x(), vector.y(), z, w} { }

template<scalar Type>
inline constexpr auto basic_vector4<Type>::dot(const basic_vector4& lhs, const basic_vector4& rhs) noexcept -> length_type {
  return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z() + lhs.w() * rhs.w();
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::normalized(const basic_vector4& vector) noexcept -> basic_vector4 {
  const auto length_squared = vector.length_squared();

  if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
    const auto length = std::sqrt(length_squared);
    return vector / length;
  }

  return vector;
}

template<scalar Type>
inline constexpr basic_vector4<Type>::operator basic_vector3<Type>() const noexcept {
  return basic_vector3<Type>{x(), y(), z()};
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::x() noexcept -> reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::x() const noexcept -> const_reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::y() noexcept -> reference {
  return base_type::operator[](y_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::y() const noexcept -> const_reference {
  return base_type::operator[](y_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::z() noexcept -> reference {
  return base_type::operator[](z_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::z() const noexcept -> const_reference {
  return base_type::operator[](z_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::w() noexcept -> reference {
  return base_type::operator[](w_axis);
}

template<scalar Type>
inline constexpr auto basic_vector4<Type>::w() const noexcept -> const_reference {
  return base_type::operator[](w_axis);
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator+(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs> {
  return lhs += rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator-(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs> {
  return lhs -= rhs;
}

template<scalar Type>
inline constexpr auto operator-(basic_vector4<Type> vector) noexcept -> basic_vector4<Type> {
  return vector *= static_cast<Type>(-1);
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector4<Lhs> lhs, Rhs scalar) noexcept -> basic_vector4<Lhs> {
  return lhs *= scalar;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs> {
  return lhs *= rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator/(basic_vector4<Lhs> lhs, Rhs scalar) noexcept -> basic_vector4<Lhs> {
  return lhs /= scalar;
}

} // namespace sbx::math

template<sbx::math::scalar Type>
inline std::size_t std::hash<sbx::math::basic_vector4<Type>>::operator()(const sbx::math::basic_vector4<Type>& vector) const noexcept {
  auto seed = std::size_t{0};

  sbx::utility::hash_combine(seed, vector.x());
  sbx::utility::hash_combine(seed, vector.y());
  sbx::utility::hash_combine(seed, vector.z());
  sbx::utility::hash_combine(seed, vector.w());

  return seed;
}

template<sbx::math::scalar Type>
template<typename ParseContext>
inline constexpr auto fmt::formatter<sbx::math::basic_vector4<Type>>::parse(ParseContext& context) noexcept -> decltype(context.begin()) {
  return context.begin();
}

template<sbx::math::scalar Type>
template<typename FormatContext>
inline auto fmt::formatter<sbx::math::basic_vector4<Type>>::format(const sbx::math::basic_vector4<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out()) {
  if constexpr (sbx::math::is_floating_point_v<Type>) {
    return fmt::format_to(context.out(), "{{x: {:.2f}, y: {:.2f}, z: {:.2f}, w: {:.2f}}}", vector.x(), vector.y(), vector.z(), vector.w());
  } else {
    return fmt::format_to(context.out(), "{{x: {}, y: {}, z: {}, w: {}}}", vector.x(), vector.y(), vector.z(), vector.w());
  }
}

template<sbx::math::scalar Type>
inline auto YAML::convert<sbx::math::basic_vector4<Type>>::encode(const sbx::math::basic_vector4<Type>& rhs) -> YAML::Node {
  auto node = Node{};

  node.SetStyle(YAML::EmitterStyle::Flow);

  node["x"] = rhs.x();
  node["y"] = rhs.y();
  node["z"] = rhs.z();
  node["w"] = rhs.w();

  return node;
}

template<sbx::math::scalar Type>
inline auto YAML::convert<sbx::math::basic_vector4<Type>>::decode(const YAML::Node& node, sbx::math::basic_vector4<Type>& rhs) -> bool {
  if (!node.IsMap()) {
    return false;
  }

  rhs.x() = node["x"].as<Type>();
  rhs.y() = node["y"].as<Type>();
  rhs.z() = node["z"].as<Type>();
  rhs.w() = node["w"].as<Type>();

  return true;
}
