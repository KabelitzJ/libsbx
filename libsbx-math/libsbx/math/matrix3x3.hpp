#ifndef LIBSBX_MATH_MATRIX3X3_HPP_
#define LIBSBX_MATH_MATRIX3X3_HPP_

#include <array>
#include <cstddef>
#include <cmath>
#include <cinttypes>
#include <concepts>
#include <fstream>
#include <ostream>
#include <type_traits>

#include <fmt/format.h>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/fwd.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/traits.hpp>

namespace sbx::math {

template<scalar Type>
class basic_matrix3x3 : public basic_matrix<3u, 3u, Type> {

  using base_type = basic_matrix<3u, 3u, Type>;

  template<scalar Other>
  using column_type_for = basic_vector3<Other>;

  inline static constexpr auto x_axis = std::size_t{0u};
  inline static constexpr auto y_axis = std::size_t{1u};
  inline static constexpr auto z_axis = std::size_t{2u};

public:

  using value_type = base_type::value_type;
  using reference = base_type::reference;
  using const_reference = base_type::const_reference;
  using size_type = base_type::size_type;
  using column_type = column_type_for<value_type>;

  inline static constexpr basic_matrix3x3 identity{base_type::identity()};
  inline static constexpr basic_matrix3x3 zero{base_type{value_type{0}}};

  using base_type::base_type;

  constexpr basic_matrix3x3(const base_type& base) noexcept;

  template<typename Column0, typename Column1, typename Column2>
  constexpr basic_matrix3x3(
    Column0&& column0,
    Column1&& column1,
    Column2&& column2
  ) noexcept;

  template<scalar Other>
  constexpr basic_matrix3x3(
    Other x0, Other x1, Other x2,
    Other y0, Other y1, Other y2,
    Other z0, Other z1, Other z2
  ) noexcept;

  template<scalar Other>
  constexpr basic_matrix3x3(const Other v00, const Other v11, const Other v22) noexcept;

  template<scalar Other>
  constexpr basic_matrix3x3(const Other diagonal) noexcept;

  // -- Static member functions --

  [[nodiscard]] constexpr static auto inverted(const basic_matrix3x3& matrix) -> basic_matrix3x3 {
    const auto m00 = matrix[0][0];
    const auto m01 = matrix[0][1];
    const auto m02 = matrix[0][2];

    const auto m10 = matrix[1][0];
    const auto m11 = matrix[1][1];
    const auto m12 = matrix[1][2];

    const auto m20 = matrix[2][0];
    const auto m21 = matrix[2][1];
    const auto m22 = matrix[2][2];

    const auto determinant = m00 * (m11 * m22 - m21 * m12) - m10 * (m01 * m22 - m21 * m02) + m20 * (m01 * m12 - m11 * m02);

    // if constexpr (std::is_floating_point_v<value_type>) {
    //   if (std::abs(determinant) < std::numeric_limits<value_type>::epsilon()) {
    //     return identity;
    //     }
    // } else {
    //   if (determinant == value_type{0}) {
    //     return identity;
    //   }
    // }

    if (comparision_traits<value_type>::equal(determinant, 0)) {
      return identity;
    }

    const auto inv_determinant = value_type{1} / determinant;

    auto result = basic_matrix3x3{};
    
    result[0][0] = (m11 * m22 - m21 * m12) * inv_determinant;
    result[0][1] = (m21 * m02 - m01 * m22) * inv_determinant;
    result[0][2] = (m01 * m12 - m11 * m02) * inv_determinant;

    result[1][0] = (m20 * m12 - m10 * m22) * inv_determinant;
    result[1][1] = (m00 * m22 - m20 * m02) * inv_determinant;
    result[1][2] = (m10 * m02 - m00 * m12) * inv_determinant;

    result[2][0] = (m10 * m21 - m20 * m11) * inv_determinant;
    result[2][1] = (m20 * m01 - m00 * m21) * inv_determinant;
    result[2][2] = (m00 * m11 - m10 * m01) * inv_determinant;

    return result;
  }

  [[nodiscard]] constexpr static auto transposed(const basic_matrix3x3& matrix) noexcept -> basic_matrix3x3 {
    auto result = basic_matrix3x3<value_type>{};

    result[0][0] = matrix[0][0];
    result[0][1] = matrix[1][0];
    result[0][2] = matrix[2][0];

    result[1][0] = matrix[0][1];
    result[1][1] = matrix[1][1];
    result[1][2] = matrix[2][1];

    result[2][0] = matrix[0][2];
    result[2][1] = matrix[1][2];
    result[2][2] = matrix[2][2];

    return result;
  }

  [[nodiscard]] constexpr static auto abs(const basic_matrix3x3& matrix) noexcept -> basic_matrix3x3 {
    return basic_matrix3x3{
      column_type::abs(matrix[0]),
      column_type::abs(matrix[1]),
      column_type::abs(matrix[2])
    };
  }

  // [[nodiscard]] constexpr static auto inverted(const basic_matrix3x3& matrix) -> basic_matrix3x3;

  // [[nodiscard]] constexpr static auto perspective(const basic_angle<value_type>& fov, const value_type aspect, const value_type near, const value_type far) noexcept -> basic_matrix3x3;

  // [[nodiscard]] constexpr static auto translated(const basic_matrix3x3& matrix, const basic_vector3<value_type>& vector) noexcept -> basic_matrix3x3;

  // [[nodiscard]] constexpr static auto scaled(const basic_matrix3x3& matrix, const basic_vector3<value_type>& vector) noexcept -> basic_matrix3x3;

  // [[nodiscard]] constexpr static auto rotated(const basic_matrix3x3& matrix, const basic_vector3<value_type>& axis, const basic_angle<value_type>& angle) noexcept -> basic_matrix3x3;

  // [[nodiscard]] constexpr static auto rotation_from_euler_angles(const basic_vector3<value_type>& euler_angles) noexcept -> basic_matrix3x3;

  constexpr auto operator[](size_type index) const noexcept -> const column_type&;

  constexpr auto operator[](size_type index) noexcept -> column_type&;

}; // class basic_matrix3x3

// template<scalar Lhs, scalar Rhs>
// [[nodiscard]] constexpr auto operator+(basic_matrix3x3<Lhs> lhs, const basic_matrix3x3<Rhs>& rhs) noexcept -> basic_matrix3x3<Lhs>;

// template<scalar Lhs, scalar Rhs>
// [[nodiscard]] constexpr auto operator-(basic_matrix3x3<Lhs> lhs, const basic_matrix3x3<Rhs>& rhs) noexcept -> basic_matrix3x3<Lhs>;

// template<scalar Lhs, scalar Rhs>
// [[nodiscard]] constexpr auto operator*(basic_matrix3x3<Lhs> lhs, Rhs scalar) noexcept -> basic_matrix3x3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_matrix3x3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs>;

template<scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_matrix3x3<Lhs> lhs, const basic_matrix3x3<Rhs>& rhs) noexcept -> basic_matrix3x3<Lhs>;

// template<scalar Lhs, scalar Rhs>
// [[nodiscard]] constexpr auto operator/(basic_matrix3x3<Lhs> lhs, Rhs scalar) noexcept -> basic_matrix3x3<Lhs>;

template<scalar Type>
struct concrete_matrix<3, 3, Type> {
  using type = basic_matrix3x3<Type>;
}; // struct concrete_matrix

using matrix3x3f = basic_matrix3x3<std::float_t>;

using matrix3x3i = basic_matrix3x3<std::int32_t>;

using matrix3x3 = matrix3x3f;

} // namespace sbx::math

template<sbx::math::scalar Type>
struct fmt::formatter<sbx::math::basic_matrix3x3<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin()) {
    return context.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::math::basic_matrix3x3<Type>& matrix, FormatContext& context) -> decltype(context.out()) {
    if constexpr (sbx::math::is_floating_point_v<Type>) {
      return fmt::format_to(context.out(), 
        "\n{:.2f}, {:.2f}, {:.2f}\n{:.2f}, {:.2f}, {:.2f}\n{:.2f}, {:.2f}, {:.2f}\n{:.2f}, {:.2f}, {:.2f}", 
        matrix[0][0], matrix[1][0], matrix[2][0],
        matrix[0][1], matrix[1][1], matrix[2][1],
        matrix[0][2], matrix[1][2], matrix[2][2]
      );
    } else {
      return fmt::format_to(context.out(), 
        "\n{}, {}, {}\n{}, {}, {}\n{}, {}, {}\n{}, {}, {}", 
        matrix[0][0], matrix[1][0], matrix[2][0],
        matrix[0][1], matrix[1][1], matrix[2][1],
        matrix[0][2], matrix[1][2], matrix[2][2]
      );
    }
  }

}; // struct fmt::formatter

#include <libsbx/math/matrix3x3.ipp>

#endif // LIBSBX_MATH_MATRIX3X3_HPP_
