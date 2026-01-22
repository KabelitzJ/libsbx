// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_FWD_HPP_
#define LIBSBX_MATH_FWD_HPP_

#include <libsbx/math/concepts.hpp>

namespace sbx::math {

namespace detail {

template<std::size_t, std::size_t, typename>
struct matrix_cast_impl;

} // namespace detail

template<std::size_t Columns, std::size_t Rows, scalar Type>
struct concrete_matrix;

template<std::size_t Columns, std::size_t Rows, scalar Type>
using concrete_matrix_t = typename concrete_matrix<Columns, Rows, Type>::type;

} // namespace sbx::math

#endif // LIBSBX_MATH_FWD_HPP_