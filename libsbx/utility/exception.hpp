// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_EXCEPTION_HPP_
#define LIBSBX_UTILITY_EXCEPTION_HPP_

#include <concepts>
#include <string_view>
#include <source_location>
#include <exception>
#include <stdexcept>

#include <fmt/format.h>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

/**
 * @brief A std::runtime_error whose message is built with fmt::format instead of being
 * passed as an already-assembled string.
 *
 * @tparam Args The types of the format arguments.
 *
 * @param fmt The fmt format string.
 * @param args The format arguments.
 */
struct runtime_error : public std::runtime_error {

  template<typename... Args>
  runtime_error(fmt::format_string<Args...> fmt, Args&&... args)
  : std::runtime_error{fmt::format(fmt, std::forward<Args>(args)...)} { }

}; // struct runtime_error

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_EXCEPTION_HPP_
