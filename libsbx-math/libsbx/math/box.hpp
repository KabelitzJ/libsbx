// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/box.hpp
 * 
 * @brief Box / frustum-style volume intersection helpers.
 *
 * @details
 *
 * This header defines a box-like structure represented by six clipping planes.
 * It can be used for intersection tests against axis-aligned volumes.
 *
 * The primary use case is view-frustum culling or generalized half-space tests
 * against bounding volumes.
 *
 * @author KAJ
 *
 * @copyright (c) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2025-05-18
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
 * @brief Plane-based box represented by six clipping planes.
 *
 * The box stores a fixed set of six planes. The intersection test uses a
 * "positive vertex" strategy against an AABB-like volume type.
 *
 * @tparam Type Scalar value type.
 */
template<scalar Type>
class basic_box {

public:

  /**
   * @brief Underlying scalar value type.
   */
  using value_type = Type;

  /**
   * @brief Plane type used by this box.
   */
  using plane_type = basic_plane<value_type>;

  /**
   * @brief Volume type tested for intersection.
   */
  using volume_type = basic_volume<value_type>;

  /**
   * @brief Index type for plane access.
   */
  using size_type = std::size_t;

  /**
   * @brief Constructs an empty box.
   */
  basic_box() noexcept = default;

  /**
   * @brief Constructs a box from a plane array.
   *
   * @param planes Plane array describing the box.
   */
  basic_box(const std::array<plane_type, 6u>& planes) noexcept;

  /**
   * @brief Constructs a box by moving in a plane array.
   *
   * @param planes Plane array describing the box.
   */
  basic_box(std::array<plane_type, 6u>&& planes) noexcept;

  /**
   * @brief Tests whether this box intersects a volume.
   *
   * The test evaluates the volume against each plane using the volume vertex
   * most aligned with the plane normal.
   *
   * @param volume Volume to test for intersection.
   *
   * @return True if the volume intersects or is inside the box.
   */
  auto intersects(const volume_type& volume) const -> bool;

  /**
   * @brief Returns the plane array backing this box.
   *
   * @return Reference to the plane array.
   */
  auto planes() const noexcept -> const std::array<plane_type, 6u>&;

  /**
   * @brief Returns the plane at a given index.
   *
   * @param index Plane index.
   *
   * @return Reference to the plane at @p index.
   */
  auto plane(const size_type index) const noexcept -> const plane_type&;

private:

  std::array<plane_type, 6u> _planes;

}; // class basic_box

/**
 * @brief Box type using @ref std::float_t.
 */
using boxf = basic_box<std::float_t>;

/**
 * @brief Default box alias.
 */
using box = boxf;

} // namespace sbx::math

#include <libsbx/math/box.ipp>

#endif // LIBSBX_MATH_BOX_HPP_
