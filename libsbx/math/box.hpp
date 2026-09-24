// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/math/box.hpp
 *
 * @brief A plane-based box for frustum-style volume intersection tests.
 *
 * @ingroup libsbx-math
 */

#ifndef LIBSBX_MATH_BOX_HPP_
#define LIBSBX_MATH_BOX_HPP_

#include <array>
#include <cstddef>
#include <utility>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/plane.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::math {

/**
 * @brief Plane-based box represented by six clipping planes, for frustum-style intersection tests against a basic_volume. The intersection test uses a "positive vertex" strategy: for each plane, only the volume's corner farthest along that plane's normal is tested, since if even that corner is outside, the whole volume is outside.
 *
 * @tparam Type Scalar value type.
 */
template<scalar Type>
class basic_box {

public:

  using value_type = Type;
  using plane_type = basic_plane<value_type>;
  using volume_type = basic_volume<value_type>;
  using size_type = std::size_t;

  basic_box() noexcept = default;

  /**
   * @brief Constructs from six clipping planes.
   *
   * @param planes The box's planes.
   */
  basic_box(const std::array<plane_type, 6u>& planes) noexcept;

  /** @copydoc basic_box(const std::array<plane_type, 6u>&) */
  basic_box(std::array<plane_type, 6u>&& planes) noexcept;

  /** @return Whether volume intersects or lies inside this box. */
  auto intersects(const volume_type& volume) const -> bool;

  auto planes() const noexcept -> const std::array<plane_type, 6u>&;

  /** @return The plane at index. */
  auto plane(const size_type index) const noexcept -> const plane_type&;

private:

  std::array<plane_type, 6u> _planes;

}; // class basic_box

using boxf = basic_box<std::float_t>;

using box = boxf;

} // namespace sbx::math

#include <libsbx/math/box.ipp>

#endif // LIBSBX_MATH_BOX_HPP_
