// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CLI_TRAITS_HPP_
#define LIBSBX_CLI_TRAITS_HPP_

#include <type_traits>
#include <vector>
#include <optional>

namespace sbx::cli {

template<typename Type>
struct is_optional : std::false_type { };

template<typename Type>
struct is_optional<std::optional<Type>> : std::true_type { };

template<typename Type>
inline constexpr auto is_optional_v = is_optional<Type>::value;

template<typename Type>
struct is_vector : std::false_type { };

template<typename Type>
struct is_vector<std::vector<Type>> : std::true_type { };

template<typename Type>
inline constexpr auto is_vector_v = is_vector<Type>::value;

} // namespace sbx::cli

#endif // LIBSBX_CLI_TRAITS_HPP_
