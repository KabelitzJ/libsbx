// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_ASSERT_HPP_
#define LIBSBX_UTILITY_ASSERT_HPP_

#include <concepts>
#include <string_view>
#include <source_location>
#include <iostream>
#include <ranges>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

/**
 * @brief Thrown by assert_that when its condition fails. Carries the fully formatted assertion message (source location, failing expression) already printed to stderr.
 *
 * @param message The formatted assertion message.
 */
struct assertion_failure : public std::runtime_error {

  explicit assertion_failure(std::string message)
  : std::runtime_error{std::move(message)} { }

}; // struct assertion_failure

namespace detail {

[[noreturn]] inline auto fail_assertion(std::string_view message, const std::source_location& source_location) -> void {
  const auto error = fmt::format("Assertion '{}' at {}:{} in '{}' failed. Terminating.\n", message, source_location.file_name(), source_location.line(), source_location.function_name());

  std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
  std::cerr.flush();

  throw assertion_failure{error};
}

inline auto warn_expectation(std::string_view message, const std::source_location& source_location) -> void {
  const auto warning = fmt::format("Expectation '{}' at {}:{} in '{}' failed.\n", message, source_location.file_name(), source_location.line(), source_location.function_name());

  std::cerr.write(warning.data(), static_cast<std::streamsize>(warning.size()));
  std::cerr.flush();
}

} // namespace detail

/**
 * @brief A compile-time checked format string plus the source location it was written at.
 *
 * Converting the format string into this captures the caller's location, which a defaulted
 * std::source_location parameter can't do once it would have to follow a parameter pack.
 *
 * @tparam Args The format arguments the string is checked against.
 */
template<typename... Args>
struct format_with_location {

  template<typename String>
  requires (std::convertible_to<const String&, fmt::format_string<Args...>>)
  consteval format_with_location(const String& format, const std::source_location& location = std::source_location::current())
  : format{format},
    location{location} { }

  fmt::format_string<Args...> format;
  std::source_location location;

}; // struct format_with_location

/**
 * @brief Asserts that an expression is true. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Expression The type of the expression to check.
 *
 * @param expression The expression to check.
 * @param message Description of the invariant being checked, used in the failure message.
 * @param source_location Where the assertion is written; defaults to the call site.
 *
 * @throws assertion_failure If expression is false.
 */
template<std::convertible_to<bool> Expression>
inline auto assert_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (is_build_type_debug_v) {
    if (!static_cast<bool>(std::forward<Expression>(expression))) {
      detail::fail_assertion(message, source_location);
    }
  }
}

/**
 * @brief Asserts that an expression is true, formatting the message only when it fails. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Expression The type of the expression to check.
 * @tparam Args The format arguments.
 *
 * @param expression The expression to check.
 * @param message Format string describing the invariant; captures the call site.
 * @param args Arguments for the format string. Still evaluated at the call site, only formatted on failure.
 *
 * @throws assertion_failure If expression is false.
 */
template<std::convertible_to<bool> Expression, typename... Args>
requires (sizeof...(Args) > 0u)
inline auto assert_that(Expression&& expression, format_with_location<std::type_identity_t<Args>...> message, Args&&... args) -> void {
  if constexpr (is_build_type_debug_v) {
    if (!static_cast<bool>(std::forward<Expression>(expression))) {
      detail::fail_assertion(fmt::format(message.format, std::forward<Args>(args)...), message.location);
    }
  }
}

/**
 * @brief Asserts that a projection is true for every element of a range. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Range The type of the range to check.
 * @tparam Project The type of the per-element predicate.
 *
 * @param range The range to check.
 * @param project Predicate invoked with each element; must return a value convertible to bool.
 * @param message Description of the invariant being checked, used in the failure message.
 * @param source_location Where the assertion is written; defaults to the call site.
 *
 * @throws assertion_failure If project returns false for any element.
 */
template<std::ranges::range Range, typename Project>
requires (std::is_invocable_r_v<bool, Project, std::ranges::range_const_reference_t<Range>>)
inline auto assert_that(Range&& range, Project&& project, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (is_build_type_debug_v) {
    for (const auto& [index, value] : std::views::enumerate(range)) {
      if (!static_cast<bool>(std::invoke(project, value))) {
        const auto error = fmt::format("Assertion '{}' at {}:{} in '{}' failed at index {}. Terminating.\n", message, source_location.file_name(), source_location.line(), source_location.function_name(), index);

        std::cerr.write(error.data(), static_cast<std::streamsize>(error.size()));
        std::cerr.flush();

        throw assertion_failure{error};
      }
    }

  }
}

/**
 * @brief Warns if an expression is false, without throwing. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Expression The type of the expression to check.
 *
 * @param expression The expression to check.
 * @param message Description of the invariant being checked, used in the warning message.
 * @param source_location Where the expectation is written; defaults to the call site.
 */
template<std::convertible_to<bool> Expression>
inline auto expect_that(Expression&& expression, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (is_build_type_debug_v) {
    if (!static_cast<bool>(expression)) {
      detail::warn_expectation(message, source_location);
    }
  }
}

/**
 * @brief Warns if an expression is false, formatting the message only when it is. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Expression The type of the expression to check.
 * @tparam Args The format arguments.
 *
 * @param expression The expression to check.
 * @param message Format string describing the expectation; captures the call site.
 * @param args Arguments for the format string. Still evaluated at the call site, only formatted on failure.
 */
template<std::convertible_to<bool> Expression, typename... Args>
requires (sizeof...(Args) > 0u)
inline auto expect_that(Expression&& expression, format_with_location<std::type_identity_t<Args>...> message, Args&&... args) -> void {
  if constexpr (is_build_type_debug_v) {
    if (!static_cast<bool>(expression)) {
      detail::warn_expectation(fmt::format(message.format, std::forward<Args>(args)...), message.location);
    }
  }
}

/**
 * @brief Warns if a projection is false for any element of a range, without throwing. Debug builds only — compiled out entirely in release builds.
 *
 * @tparam Range The type of the range to check.
 * @tparam Project The type of the per-element predicate.
 *
 * @param range The range to check.
 * @param project Predicate invoked with each element; must return a value convertible to bool.
 * @param message Description of the invariant being checked, used in the warning message.
 * @param source_location Where the expectation is written; defaults to the call site.
 */
template<std::ranges::range Range, typename Project>
requires (std::is_invocable_r_v<bool, Project, std::ranges::range_const_reference_t<Range>>)
inline auto expect_that(Range&& range, Project&& project, std::string_view message, const std::source_location& source_location = std::source_location::current()) -> void {
  if constexpr (is_build_type_debug_v) {
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
