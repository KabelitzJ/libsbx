// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ASSERT_HPP_
#define LIBSBX_UTILITY_ASSERT_HPP_

#include <concepts>
#include <string_view>
#include <source_location>
#include <iostream>

#include <fmt/format.h>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

template<std::convertible_to<bool> Expression>
inline auto assert_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    if (!static_cast<bool>(expression)) {
      const auto error = fmt::format("Assertion '{}' at {}:{} in '{}' failed. Terminating", message, source_location.file_name(), source_location.line(), source_location.function_name());
      std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
      std::cerr.flush();
      std::terminate();
    }
  }
}

template<std::convertible_to<bool> Expression>
inline auto expect_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    if (!static_cast<bool>(expression)) {
      const auto warning = fmt::format("Expectation '{}' at {}:{} in '{}' failed.", message, source_location.file_name(), source_location.line(), source_location.function_name());
      std::cerr.write(warning.data(), static_cast<std::streamsize>(warning.size()));
      std::cerr.flush();
    }
  }
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ASSERT_HPP_
