// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_EXCEPTION_HPP_
#define LIBSBX_UTILITY_EXCEPTION_HPP_

#include <concepts>
#include <string_view>
#include <source_location>
#include <iostream>
#include <exception>
#include <stdexcept>

#include <fmt/format.h>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

struct runtime_error : public std::runtime_error {

  template<typename... Args>
  runtime_error(fmt::format_string<Args...> fmt, Args&&... args)
  : std::runtime_error{fmt::format(fmt, std::forward<Args>(args)...)} { }

}; // struct runtime_error

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_EXCEPTION_HPP_
