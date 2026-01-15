#include <libsbx/math/vector3.hpp>

namespace sbx::math {

template<scalar Type>
inline constexpr basic_vector3<Type>::basic_vector3(const base_type& base) noexcept
: base_type{base} { }

template<scalar Type>
template<scalar X, scalar Y, scalar Z>
inline constexpr basic_vector3<Type>::basic_vector3(X x, Y y, Z z) noexcept
: base_type{x, y, z} { }

template<scalar Type>
template<scalar Other, scalar Scalar>
inline constexpr basic_vector3<Type>::basic_vector3(const basic_vector2<Other>& vector, Scalar z) noexcept
: base_type{vector.x(), vector.y(), z} { }

template<scalar Type>
inline constexpr auto basic_vector3<Type>::cross(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> basic_vector3 {
  return basic_vector3{
    lhs.y() * rhs.z() - lhs.z() * rhs.y(),
    lhs.z() * rhs.x() - lhs.x() * rhs.z(),
    lhs.x() * rhs.y() - lhs.y() * rhs.x()
  };
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::dot(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> length_type {
  return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z();
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::normalized(const basic_vector3& vector) noexcept -> basic_vector3 {
  const auto length_squared = vector.length_squared();

  if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
    const auto length = std::sqrt(length_squared);

    return vector / length;
  }

  return vector;
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::reflect(const basic_vector3& vector, const basic_vector3& normal) noexcept -> basic_vector3 {
  return vector - normal * (dot(vector, normal) * 2);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::abs(const basic_vector3& vector) noexcept -> basic_vector3 {
  return basic_vector3{std::abs(vector.x()), std::abs(vector.y()), std::abs(vector.z())};
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::distance_squared(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> value_type {
  return (lhs - rhs).length_squared();
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::distance(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> value_type {
  return (lhs - rhs).length();
}

template<scalar Type>
inline constexpr basic_vector3<Type>::operator basic_vector2<Type>() const noexcept {
  return basic_vector2<Type>{x(), y()};
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::x() noexcept -> reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::x() const noexcept -> const_reference {
  return base_type::operator[](x_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::y() noexcept -> reference {
  return base_type::operator[](y_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::y() const noexcept -> const_reference {
  return base_type::operator[](y_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::z() noexcept -> reference {
  return base_type::operator[](z_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::z() const noexcept -> const_reference {
  return base_type::operator[](z_axis);
}

template<scalar Type>
inline constexpr auto basic_vector3<Type>::normalize() noexcept -> basic_vector3& {
  return static_cast<basic_vector3&>(base_type::normalize());
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator+(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs> {
  return lhs += rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator-(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs> {
  return lhs -= rhs;
}

template<scalar Type>
inline constexpr auto operator-(basic_vector3<Type> vector) noexcept -> basic_vector3<Type> {
  return vector *= static_cast<Type>(-1);
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector3<Lhs> lhs, Rhs scalar) noexcept -> basic_vector3<Lhs> {
  return lhs *= scalar;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(Lhs scalar, basic_vector3<Rhs> rhs) noexcept -> basic_vector3<Rhs> {
  return rhs *= scalar;
}

template<scalar Lhs, std::convertible_to<Lhs> Rhs>
requires (!is_scalar_v<Rhs>)
inline constexpr auto operator*(basic_vector3<Lhs> lhs, const Rhs& rhs) noexcept -> basic_vector3<Lhs> {
  return lhs *= static_cast<Lhs>(rhs);
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs> {
  return basic_vector3<Lhs>{lhs.x() * rhs.x(), lhs.y() * rhs.y(), lhs.z() * rhs.z()};
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator/(basic_vector3<Lhs> lhs, Rhs scalar) noexcept -> basic_vector3<Lhs> {
  return lhs /= scalar;
}

template<scalar Lhs, std::convertible_to<Lhs> Rhs>
requires (!is_scalar_v<Rhs>)
inline constexpr auto operator/(basic_vector3<Lhs> lhs, const Rhs& rhs) noexcept -> basic_vector3<Lhs> {
  return lhs /= static_cast<Lhs>(rhs);
}

} // namespace sbx::math

template<sbx::math::scalar Type>
inline auto std::hash<sbx::math::basic_vector3<Type>>::operator()(const sbx::math::basic_vector3<Type>& vector) const noexcept -> std::size_t {
  auto seed = std::size_t{0};

  sbx::utility::hash_combine(seed, vector.x(), vector.y(), vector.z());

  return seed;
}

template<sbx::math::scalar Type>
template<typename ParseContext>
inline constexpr auto fmt::formatter<sbx::math::basic_vector3<Type>>::parse(ParseContext& context) noexcept -> decltype(context.begin()) {
  return context.begin();
}

template<sbx::math::scalar Type>
template<typename FormatContext>
inline auto fmt::formatter<sbx::math::basic_vector3<Type>>::format(const sbx::math::basic_vector3<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out()) {
  if constexpr (sbx::math::is_floating_point_v<Type>) {
    return fmt::format_to(context.out(), "{{x: {:.2f}, y: {:.2f}, z: {:.2f}}}", vector.x(), vector.y(), vector.z());
  } else {
    return fmt::format_to(context.out(), "{{x: {}, y: {}, z: {}}}", vector.x(), vector.y(), vector.z());
  }
}

// template<sbx::math::scalar Type>
// inline auto YAML::convert<sbx::math::basic_vector3<Type>>::encode(const sbx::math::basic_vector3<Type>& rhs) -> YAML::Node {
//   auto node = Node{};

//   node.SetStyle(YAML::EmitterStyle::Flow);

//   node["x"] = rhs.x();
//   node["y"] = rhs.y();
//   node["z"] = rhs.z();

//   return node;
// }

// template<sbx::math::scalar Type>
// inline auto YAML::convert<sbx::math::basic_vector3<Type>>::decode(const YAML::Node& node, sbx::math::basic_vector3<Type>& rhs) -> bool {
//   if (!node.IsMap()) {
//     return false;
//   }

//   rhs.x() = node["x"].as<Type>();
//   rhs.y() = node["y"].as<Type>();
//   rhs.z() = node["z"].as<Type>();

//   return true;
// }
