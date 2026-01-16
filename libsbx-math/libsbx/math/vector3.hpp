#ifndef LIBSBX_MATH_VECTOR3_HPP_
#define LIBSBX_MATH_VECTOR3_HPP_

#include <concepts>
#include <cstddef>
#include <cmath>
#include <fstream>
#include <ostream>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector.hpp>
#include <libsbx/math/vector2.hpp>

namespace sbx::math {

template<scalar Type>
class basic_vector3 : public basic_vector<3u, Type> {

  using base_type = basic_vector<3u, Type>;

  inline static constexpr auto x_axis = std::size_t{0u};
  inline static constexpr auto y_axis = std::size_t{1u};
  inline static constexpr auto z_axis = std::size_t{2u};

public:

  using value_type = base_type::value_type;
  using reference = base_type::reference;
  using const_reference = base_type::const_reference;
  using size_type = base_type::size_type;
  using length_type = base_type::length_type;

  inline static constexpr basic_vector3 zero{base_type::fill(value_type{0})};
  inline static constexpr basic_vector3 one{base_type::fill(value_type{1})};
  inline static constexpr basic_vector3 right{base_type::template axis<x_axis>(value_type{1})};
  inline static constexpr basic_vector3 left{base_type::template axis<x_axis>(value_type{-1})};
  inline static constexpr basic_vector3 up{base_type::template axis<y_axis>(value_type{1})};
  inline static constexpr basic_vector3 down{base_type::template axis<y_axis>(value_type{-1})};
  inline static constexpr basic_vector3 forward{base_type::template axis<z_axis>(value_type{-1})};
  inline static constexpr basic_vector3 backward{base_type::template axis<z_axis>(value_type{1})};

  using base_type::base_type;

  constexpr basic_vector3(const base_type& base) noexcept;

  template<scalar X, scalar Y, scalar Z>
  constexpr basic_vector3(X x, Y y, Z z) noexcept;

  template<scalar Other, scalar Scalar = Other>
  constexpr basic_vector3(const basic_vector2<Other>& vector, Scalar z = Scalar{0}) noexcept;

  [[nodiscard]] static constexpr auto cross(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> basic_vector3;

  [[nodiscard]] static constexpr auto dot(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> length_type;

  [[nodiscard]] static constexpr auto normalized(const basic_vector3& vector) noexcept -> basic_vector3;

  [[nodiscard]] static constexpr auto reflect(const basic_vector3& vector, const basic_vector3& normal) noexcept -> basic_vector3;

  // [[nodiscard]] static constexpr auto abs(const basic_vector3& vector) noexcept -> basic_vector3;

  [[nodiscard]] static constexpr auto distance_squared(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> value_type;

  [[nodiscard]] static constexpr auto distance(const basic_vector3& lhs, const basic_vector3& rhs) noexcept -> value_type;

  [[nodiscard]] static constexpr auto orthogonal(const basic_vector3& vector) noexcept -> basic_vector3 {
    if (vector.length_squared() < 1e-12f) {
      return {1.0f, 0.0f, 0.0f};
    }

    const auto ax = std::abs(vector.x());
    const auto ay = std::abs(vector.y());
    const auto az = std::abs(vector.z());

    // choose the axis least aligned with vector
    auto other = (ax < ay && ax < az) ? basic_vector3{1, 0, 0} : (ay < az) ? basic_vector3{0, 1, 0} : basic_vector3{0, 0, 1};

    auto orthogonal = cross(vector, other);

    // last resort (should almost never happen)
    if (orthogonal.length_squared() < 1e-12f) {
      other = basic_vector3{0, 1, 0};
      orthogonal = cross(vector, other);
    }

    return normalized(orthogonal);
  }

  [[nodiscard]] static constexpr auto splat_x(const basic_vector3& vector) noexcept -> basic_vector3 {
    return base_type::template splat<x_axis>(vector);
  }

  [[nodiscard]] static constexpr auto splat_y(const basic_vector3& vector) noexcept -> basic_vector3 {
    return base_type::template splat<y_axis>(vector);
  }

  [[nodiscard]] static constexpr auto splat_z(const basic_vector3& vector) noexcept -> basic_vector3 {
    return base_type::template splat<z_axis>(vector);
  }

  // /**
  //  * @brief Linearly interpolates between two vectors.
  //  * 
  //  * @param start The starting vector.
  //  * @param end The ending vector.
  //  * @param t The interpolation factor [0.0f, 1.0f].
  //  * 
  //  * @return A new vector that is the result of the linear interpolation.
  //  */
  // [[nodiscard]] static constexpr auto lerp(const basic_vector3& start, const basic_vector3& end, const value_type t) noexcept -> basic_vector3 {
  //   utility::assert_that(t >= 0.0f && t <= 1.0f, "Interpolation factor out of bounds in vector3 lerp");
  //   return start * (1.0f - t) + end * t;
  // }

  [[nodiscard]] constexpr operator basic_vector2<Type>() const noexcept;

  [[nodiscard]] constexpr auto x() noexcept -> reference;

  [[nodiscard]] constexpr auto x() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto y() noexcept -> reference;

  [[nodiscard]] constexpr auto y() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto z() noexcept -> reference;

  [[nodiscard]] constexpr auto z() const noexcept -> const_reference;

  constexpr auto normalize() noexcept -> basic_vector3&;

}; // template<scalar Type>

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs>;

template<scalar Type>
[[nodiscard]] constexpr auto operator-(basic_vector3<Type> vector) noexcept -> basic_vector3<Type>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector3<Lhs> lhs, Rhs scalar) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(Lhs scalar, basic_vector3<Rhs> rhs) noexcept -> basic_vector3<Rhs>;

template<scalar Lhs, std::convertible_to<Lhs> Rhs>
requires (!is_scalar_v<Rhs>)
[[nodiscard]] constexpr auto operator*(basic_vector3<Lhs> lhs, const Rhs& rhs) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator/(basic_vector3<Lhs> lhs, Rhs scalar) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, std::convertible_to<Lhs> Rhs>
requires (!is_scalar_v<Rhs>)
[[nodiscard]] constexpr auto operator/(basic_vector3<Lhs> lhs, const Rhs& rhs) noexcept -> basic_vector3<Lhs>;

using vector3f = basic_vector3<std::float_t>;

using vector3u = basic_vector3<std::uint32_t>;

using vector3i = basic_vector3<std::int32_t>;

using vector3 = vector3f;

} // namespace sbx::math

template<sbx::math::scalar Type>
struct std::hash<sbx::math::basic_vector3<Type>> {

  inline auto operator()(const sbx::math::basic_vector3<Type>& vector) const noexcept -> std::size_t;

}; // struct std::hash<sbx::math::basic_vector3<Type>>

template<sbx::math::scalar Type>
struct fmt::formatter<sbx::math::basic_vector3<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) noexcept -> decltype(context.begin());

  template<typename FormatContext>
  auto format(const sbx::math::basic_vector3<Type>& vector, FormatContext& context) const noexcept -> decltype(context.out());

}; // struct fmt::formatter<sbx::math::basic_vector3<Type>>

template<sbx::math::scalar Type>
struct YAML::convert<sbx::math::basic_vector3<Type>> {

  static auto encode(const sbx::math::basic_vector3<Type>& rhs) -> YAML::Node {
    auto node = Node{};

    node.SetStyle(YAML::EmitterStyle::Flow);

    node["x"] = rhs.x();
    node["y"] = rhs.y();
    node["z"] = rhs.z();

    return node;
  }

  static auto decode(const YAML::Node& node, sbx::math::basic_vector3<Type>& rhs) -> bool {
    if (!node.IsMap()) {
      return false;
    }

    rhs.x() = node["x"].as<Type>();
    rhs.y() = node["y"].as<Type>();
    rhs.z() = node["z"].as<Type>();

    return true;
  }

}; // struct YAML::convert<sbx::math::basic_vector3<Type>>

template<sbx::math::scalar Type>
auto operator<<(YAML::Emitter& out, const sbx::math::basic_vector3<Type>& vector) -> YAML::Emitter& {
  return out << YAML::convert<sbx::math::basic_vector3<Type>>::encode(vector);
}

#include <libsbx/math/vector3.ipp>

#endif // LIBSBX_MATH_VECTOR3_HPP_

