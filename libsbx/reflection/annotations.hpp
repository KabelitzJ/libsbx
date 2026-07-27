// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ANNOTATIONS_HPP_
#define LIBSBX_REFLECTION_ANNOTATIONS_HPP_

#include <meta>

namespace sbx::reflection {

namespace detail {

struct reflected_tag {};
struct bit_field_tag {};

} // namespace detail

inline constexpr auto reflected = detail::reflected_tag{};
inline constexpr auto bit_field = detail::bit_field_tag{};

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ANNOTATIONS_HPP_