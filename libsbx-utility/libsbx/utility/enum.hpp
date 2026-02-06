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

template<auto... Values>
requires (std::is_enum_v<decltype(Values)> && ...)
struct enum_list {

  inline static constexpr auto values = std::array{Values...};

  inline static constexpr auto contains(const typename decltype(values)::value_type value) noexcept -> bool {
    for (auto entry : values) {
      if (entry == value) {
        return true;
      }
    }
  
    return false;
  }

  inline static constexpr auto size() noexcept -> std::size_t { 
    return values.size(); 
  }

  inline static constexpr auto is_empty() noexcept -> std::size_t { 
    return size() == 0u;
  }

}; // struct enum_list

template<>
struct enum_list<> {

  template<typename Type>
  requires (std::is_enum_v<Type>)
  inline static constexpr auto contains(const Type value) noexcept -> bool {
    return false;
  }

  inline static constexpr auto size() noexcept -> std::size_t { 
    return 0u; 
  }

  inline static constexpr auto is_empty() noexcept -> std::size_t { 
    return true;
  }

}; // struct enum_list

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

  constexpr auto set(const value_type value) noexcept -> void {
    _value |= static_cast<underlying_type>(value);
  }

  constexpr auto clear(const value_type value) noexcept -> void {
    _value &= ~static_cast<underlying_type>(value);
  }

  constexpr auto override(const value_type value) noexcept -> void {
    _value = value;
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

template<typename Enum>
requires (std::is_enum_v<Enum>)
struct entry {
  Enum value;
  std::string_view name;
}; // struct entry

template<typename Enum>
requires (std::is_enum_v<Enum>)
struct enum_mapping;


template<typename Type>
concept mapped_enum = requires() {
  std::is_enum_v<Type>;
  { enum_mapping<Type>::values };
}; // concept mapped_enum

template<mapped_enum Enum>
constexpr auto to_string(const Enum value) -> std::string_view {
  auto entry = std::ranges::find_if(enum_mapping<Enum>::values, [&value](const auto& entry){ return entry.value == value; });

  if (entry == std::ranges::end(enum_mapping<Enum>::values)) {
    return "<unknown>";
  }

  return entry->name;
}

template<mapped_enum Enum>
constexpr auto from_string(const std::string_view name) -> std::optional<Enum> {
  auto entry = std::ranges::find_if(enum_mapping<Enum>::values, [&name](const auto& entry){ return entry.name == name; });

  if (entry == std::ranges::end(enum_mapping<Enum>::values)) {
    return std::nullopt;
  }

  return entry->value;
}

} // namespace sbx::utility

template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator|(const Type lhs, const Type rhs) -> Type {
  return sbx::utility::from_underlying<Type>(sbx::utility::to_underlying(lhs) | sbx::utility::to_underlying(rhs));
}


template<typename Type>
requires (sbx::utility::is_bit_field_v<Type>)
constexpr auto operator&(const Type lhs, const Type rhs) -> Type {
  return sbx::utility::from_underlying<Type>(sbx::utility::to_underlying(lhs) & sbx::utility::to_underlying(rhs));
}

#endif // LIBSBX_UTILITY_ENUM_HPP_
