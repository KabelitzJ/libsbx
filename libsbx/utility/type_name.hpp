// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_TYPE_NAME_HPP_
#define LIBSBX_UTILITY_TYPE_NAME_HPP_

#include <string_view>

#include <libsbx/utility/target.hpp>

namespace sbx::utility {

namespace detail {

constexpr auto parse_type_name(std::string_view prefix, std::string_view suffix, std::string_view function) -> std::string_view {
  const auto start = function.find(prefix) + prefix.size();
  const auto end = function.find(suffix);
  const auto size = end - start;

  return function.substr(start, size);
}

} // namespace detail

template<typename Type>
constexpr auto type_name() -> std::string_view {
#if defined(__clang__)
  constexpr auto prefix = std::string_view{"[Type = "};
  constexpr auto suffix = "]";
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};

  return detail::parse_type_name(prefix, suffix, function);
#elif (defined(__GNUC__) || defined(__GNUG__) || defined(__MINGW32__))
  constexpr auto prefix = std::string_view{"with Type = "};
  constexpr auto suffix = "; ";
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};

  return detail::parse_type_name(prefix, suffix, function);
#elif defined(__MSC_VER)
  constexpr auto prefix = std::string_view{"type_name<"};
  constexpr auto suffix = ">(void)";
  constexpr auto function = std::string_view{__FUNCSIG__};

  return detail::parse_type_name(prefix, suffix, function);
#else
  return typeid(Type).name(); 
#endif
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_TYPE_NAME_HPP_
