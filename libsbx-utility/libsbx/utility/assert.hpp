// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ASSERT_HPP_
#define LIBSBX_UTILITY_ASSERT_HPP_

#include <concepts>
#include <string_view>
#include <source_location>
#include <iostream>
#include <ranges>
#include <functional>

#include <fmt/format.h>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

template<std::convertible_to<bool> Expression>
inline auto assert_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    if (!static_cast<bool>(std::forward<Expression>(expression))) {
      const auto error = fmt::format("Assertion '{}' at {}:{} in '{}' failed. Terminating.\n", message, source_location.file_name(), source_location.line(), source_location.function_name());

      std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
      std::cerr.flush();

      std::terminate();
    }
  }
}

template<std::ranges::range Range, typename Project>
requires (std::is_invocable_r_v<bool, Project, std::ranges::range_const_reference_t<Range>>)
inline auto assert_that(Range&& range, Project&& project, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    for (const auto& [index, value] : std::views::enumerate(range)) {
      if (!static_cast<bool>(std::invoke(project, value))) {
        const auto error = fmt::format("Assertion '{}' at {}:{} in '{}' failed at index {}. Terminating.\n", message, source_location.file_name(), source_location.line(), source_location.function_name(), index);

        std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
        std::cerr.flush();

        std::terminate();
      }
    }

  }
}

template<std::convertible_to<bool> Expression>
inline auto expect_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    if (!static_cast<bool>(expression)) {
      const auto warning = fmt::format("Expectation '{}' at {}:{} in '{}' failed.\n", message, source_location.file_name(), source_location.line(), source_location.function_name());

      std::cerr.write(warning.data(), static_cast<std::streamsize>(warning.size()));
      std::cerr.flush();
    }
  }
}

template<std::ranges::range Range, typename Project>
requires (std::is_invocable_r_v<bool, Project, std::ranges::range_const_reference_t<Range>>)
inline auto expect_that(Range&& range, Project&& project, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (build_configuration_v == build_configuration::debug) {
    for (const auto& [index, value] : std::views::enumerate(range)) {
      if (!static_cast<bool>(std::invoke(project, value))) {
        const auto error = fmt::format("Expectation '{}' at {}:{} in '{}' failed at index {}.\n", message, source_location.file_name(), source_location.line(), source_location.function_name(), index);

        std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
        std::cerr.flush();
      }
    }

  }
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ASSERT_HPP_
