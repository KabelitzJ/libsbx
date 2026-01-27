// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/matrix_cast.hpp
 * 
 * @brief Matrix casting and decomposition utilities.
 *
 * @details
 * 
 * This header provides a generic, extensible mechanism for casting between
 * different matrix representations, as well as helpers for extracting
 * transformation components from matrices.
 *
 * Supported conversions include:
 * - 3x3 <-> 4x4 matrix casting
 * - quaternion -> matrix casting
 * - Matrix decomposition into position, rotation, and scale
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2025-08-25
 */

#ifndef LIBSBX_MATH_MATRIX_CAST_HPP_
#define LIBSBX_MATH_MATRIX_CAST_HPP_

#include <type_traits>
#include <concepts>
#include <utility>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/matrix.hpp>
#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>

namespace sbx::math {

/**
 * @brief Concept describing a valid dispatcher implementation.
 *
 * @tparam Type   Dispatcher type.
 * @tparam Return Expected return type.
 * @tparam Args   Argument types passed to `invoke`.
 */
template<typename Type, typename Return, typename... Args>
concept dispatcher_for = requires() {
  { std::remove_cvref_t<Type>::invoke(std::declval<Args>()...) } -> std::same_as<Return>;
};

/*! @cond TURN_OFF_DOXYGEN */
namespace detail {

/**
 * @brief Primary template for matrix cast dispatch.
 *
 * @tparam To   Target type.
 * @tparam From Source type.
 */
template<typename To, typename From>
struct matrix_cast_impl;

} // namespace detail
/*! @endcond */

/**
 * @brief Casts between matrix-related types.
 *
 * @tparam To   Target type.
 * @tparam From Source type.
 *
 * @param from Source value.
 *
 * @return Converted value.
 */
template<typename To, typename From>
requires (dispatcher_for<detail::matrix_cast_impl<To, std::remove_cvref_t<From>>, To, From>)
[[nodiscard]] constexpr auto matrix_cast(const From& from) -> To;

/**
 * @brief Result of matrix decomposition.
 */
struct decompose_result {
  vector3 position;
  quaternion rotation;
  vector3 scale;
}; // struct decompose_result

/**
 * @brief Decomposes a transformation matrix into position, rotation, and scale.
 *
 * @param matrix Input transformation matrix.
 *
 * @return Decomposed transformation components.
 */
[[nodiscard]] auto decompose(const matrix4x4& matrix) noexcept -> decompose_result;

} // namespace sbx::math

#include <libsbx/math/matrix_cast.ipp>

#endif // LIBSBX_MATH_MATRIX_CAST_HPP_
