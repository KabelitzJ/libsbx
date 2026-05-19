// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ENUM_HPP_
#define LIBSBX_UTILITY_ENUM_HPP_

#include <type_traits>
#include <array>
#include <ranges>
#include <optional>
#include <string_view>

#include <libsbx/utility/string_literal.hpp>

namespace sbx::utility {

template<typename Enum>
requires (std::is_enum_v<Enum>)
constexpr auto to_underlying(const Enum value) -> std::underlying_type_t<Enum> {
  return static_cast<std::underlying_type_t<Enum>>(value);
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
constexpr auto from_underlying(const std::underlying_type_t<Enum> value) -> Enum {
  return static_cast<Enum>(value);
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
struct is_bit_field : std::false_type { };

template<typename Enum>
requires (std::is_enum_v<Enum>)
inline constexpr auto is_bit_field_v = is_bit_field<Enum>::value;

template<std::size_t Shift>
struct bit : std::integral_constant<std::size_t, (std::size_t{1} << Shift)> { };

template<std::size_t Shift>
inline constexpr auto bit_v = bit<Shift>::value;

template<typename Enum, typename Underlying = std::underlying_type_t<Enum>>
requires (std::is_enum_v<Enum>)
class bit_field {

public:

  using value_type = Enum;
  using underlying_type = Underlying;

  constexpr bit_field() noexcept
  : _value{underlying_type{0}} { }

  constexpr bit_field(const value_type value) noexcept
  : _value{to_underlying(value)} { }

  explicit constexpr bit_field(const underlying_type value) noexcept
  : _value{value} { }

  constexpr auto set(const value_type value) noexcept -> bit_field& {
    return *this |= value;
  }

  constexpr auto operator|=(const value_type value) noexcept -> bit_field& {
    _value |= static_cast<underlying_type>(value);

    return *this;
  }

  constexpr auto clear(const value_type value) noexcept -> void {
    _value &= ~static_cast<underlying_type>(value);
  }

  constexpr auto override(const value_type value) noexcept -> void {
    _value = static_cast<underlying_type>(value);
  }

  constexpr auto has(const value_type value) const noexcept -> bool {
    return _value & static_cast<underlying_type>(value);
  }

  constexpr auto has_any() const noexcept -> bool {
    return _value != underlying_type{0};
  }

  constexpr auto has_none() const noexcept -> bool {
    return _value == underlying_type{0};
  }

  constexpr auto operator*() const noexcept -> value_type {
    return from_underlying<value_type>(_value);
  }

  constexpr auto value() const -> value_type {
    return static_cast<value_type>(_value);
  }

  constexpr auto underlying() const -> underlying_type {
    return _value;
  }

private:

  underlying_type _value;

}; // class bit_field

} // namespace sbx::utility

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator|(Type lhs, Type rhs) -> Type {
  return sbx::utility::from_underlying<Type>(sbx::utility::to_underlying(lhs) | sbx::utility::to_underlying(rhs));
}

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator|=(Type& lhs, Type rhs) -> Type& {
  lhs = lhs | rhs;

  return lhs;
}

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator&(Type lhs, Type rhs) -> Type {
  return sbx::utility::from_underlying<Type>(sbx::utility::to_underlying(lhs) & sbx::utility::to_underlying(rhs));
}

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator&=(Type& lhs, Type rhs) -> Type& {
  lhs = lhs & rhs;

  return lhs;
}

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator^(Type lhs, Type rhs) -> Type {
  return sbx::utility::from_underlying<Type>(sbx::utility::to_underlying(lhs) ^ sbx::utility::to_underlying(rhs));
}

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator~(Type lhs) -> Type {
  return sbx::utility::from_underlying<Type>(~sbx::utility::to_underlying(lhs));
}

#endif // LIBSBX_UTILITY_ENUM_HPP_
