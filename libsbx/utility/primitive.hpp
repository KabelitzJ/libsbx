// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_PRIMITIVE_HPP_
#define LIBSBX_UTILITY_PRIMITIVE_HPP_

#include <concepts>
#include <type_traits>

#include <fmt/format.h>

#include <libsbx/utility/string_literal.hpp>

namespace sbx::utility {

template<typename Type, string_literal Unit>
requires (std::is_arithmetic_v<Type>)
class primitive {

public:

  using value_type = Type;

  inline static constexpr auto unit = Unit;

  constexpr explicit primitive(Type value = static_cast<value_type>(0)) noexcept
  : _value{value} { }

  constexpr ~primitive() = default;

  constexpr operator Type() const noexcept {
    return _value;
  }

private:

  Type _value;

}; // class primitive

} // namespace sbx::utility

template<typename Type, sbx::utility::string_literal Unit>
struct fmt::formatter<sbx::utility::primitive<Type, Unit>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin()) {
    return context.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::utility::primitive<Type, Unit>& value, FormatContext& context) const -> decltype(context.out()) {
    return fmt::format_to(context.out(), "{}{}", static_cast<Type>(value), Unit);
  }

}; // struct fmt::formatter<sbx::utility::primitive<Type>>

#endif // LIBSBX_UTILITY_PRIMITIVE_HPP_
