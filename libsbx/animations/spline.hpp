// SPDX-License-Identifier: MIT
/**
 * @file libsbx/animations/spline.hpp
 *
 * @brief Generic time-based spline container for animation sampling.
 *
 * @details
 *
 * This header defines a templated spline class used to store timestamped values
 * and to sample interpolated values at arbitrary points in time.
 *
 * The spline is designed for animation systems and supports generic value types,
 * including scalars, vectors, and quaternions, provided that appropriate
 * interpolation logic is available in the implementation.
 *
 * Timestamps are stored in ascending order and paired with corresponding values.
 * Sampling behavior outside the defined timestamp range is implementation-defined.
 *
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::animations
 * @version 0.1.0
 * @since 2024-01-06
 */

#ifndef LIBSBX_ANIMATIONS_SPLINE_HPP_
#define LIBSBX_ANIMATIONS_SPLINE_HPP_

#include <vector>
#include <algorithm>

#include <libsbx/utility/assert.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::animations {

/**
 * @class spline
 *
 * @brief Time-ordered spline for interpolated animation values.
 *
 * @tparam Type Value type stored in the spline.
 *
 * @details
 *
 * The spline class maintains a sequence of keyframes, each consisting of a
 * timestamp and a corresponding value. Values can be sampled continuously
 * over time using interpolation defined in the implementation.
 *
 * This class does not enforce a specific interpolation strategy at the
 * interface level; instead, behavior is specialized in the accompanying
 * implementation file.
 *
 * Typical use cases include animation curves for transforms, scalar parameters,
 * and orientation data.
 */
template<typename Type>
class spline {

public:

  /**
   * @brief Add a keyframe to the spline.
   *
   * @details
   *
   * Inserts a new keyframe into the spline. Implementations are expected to
   * maintain ordering of timestamps and associated values.
   * 
   * @param timestamp Time position of the keyframe.
   * @param value Value associated with the timestamp.
   */
  auto add(const std::float_t timestamp, const Type& value) -> void;

  /**
   * @brief Sample the spline at a given time.
   *
   * @details
   *
   * Returns a value interpolated between surrounding keyframes based on the
   * provided time. Behavior when sampling outside the range of stored
   * timestamps is implementation-defined.
   *
   * @param time Time value to sample.
   * 
   * @return Interpolated value at the given time.
   */
  auto sample(const std::float_t time) const -> Type;

  /**
   * @brief Retrieve the number of keyframes stored.
   *
   * @return Number of timestamped values in the spline.
   */
  auto size() const noexcept -> std::size_t;

private:

  std::vector<std::float_t> _timestamps;
  std::vector<Type> _values;

}; // class spline

} // namespace sbx::animations

#include <libsbx/animations/spline.ipp>

#endif // LIBSBX_ANIMATIONS_SPLINE_HPP_
