// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_STRING_LITERAL_HPP_
#define LIBSBX_UTILITY_STRING_LITERAL_HPP_

#include <utility>
#include <string_view>
#include <array>
#include <limits>

#include <fmt/format.h>

#include <libsbx/utility/hash.hpp>

namespace sbx::utility {

namespace detail {

template<std::forward_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel, std::forward_iterator OutputIterator>
constexpr auto copy(InputIterator first, Sentinel last, OutputIterator result) -> OutputIterator {
  while (first != last) {
    *result++ = *first++;
  }

  return result;
}

} // namespace detail

template<typename Character, std::size_t Size>
class basic_string_literal {

public:

  using character_type = Character;
  using size_type = std::size_t;
  using iterator = const character_type*;
  using string_view_type = std::basic_string_view<character_type>;
  using string_type = std::basic_string<character_type>;

  static constexpr auto npos = std::numeric_limits<size_type>::max();

  consteval basic_string_literal(const character_type (&data)[Size]) noexcept {
    detail::copy(data, data + Size - 1, _data.data());
  }

  constexpr auto begin() const noexcept -> iterator {
    return std::begin(_data);
  }

  constexpr auto end() const noexcept -> iterator {
    return std::end(_data.data());
  }

  constexpr auto data() const noexcept -> const character_type* {
    return _data.data();
  }

  constexpr auto size() const noexcept -> size_type {
    return Size - 1;
  }

  constexpr auto is_empty() const noexcept -> bool {
    return size() == 0;
  }

  constexpr auto operator[](const size_type index) const noexcept -> character_type {
    return _data[index];
  }

  constexpr auto at(const size_type index) const noexcept -> character_type {
    return _data.at(index);
  }

  constexpr auto hash() const noexcept -> std::size_t {
    return fnv1a_hash<character_type, std::size_t>{}({_data.data(), _data.size()});
  }

  constexpr operator string_view_type() const noexcept {
    return string_view_type{_data.data(), Size};
  }

  constexpr operator string_type() const noexcept {
    return (Size != 0u) ? string_type{_data.data(), Size} : std::string{};
  }

  std::array<character_type, Size - 1> _data;

}; // class basic_string_literal

template<std::size_t Size>
using string_literal = basic_string_literal<char, Size>;

template<std::size_t Size>
using wstring_literal = basic_string_literal<wchar_t, Size>;

template<string_literal String>
constexpr auto string_id() noexcept -> std::size_t {
  return String.hash();
}

} // namespace sbx::utility

template<std::size_t Size>
struct fmt::formatter<sbx::utility::string_literal<Size>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin()) {
    return context.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::utility::string_literal<Size>& value, FormatContext& context) const -> decltype(context.out()) {
    return fmt::format_to(context.out(), "{}", std::string{value.data(), value.size()});
  }

}; // struct fmt::formatter<sbx::utility::primitive<Type>>

template<typename Character, size_t Size>
struct std::hash<sbx::utility::basic_string_literal<Character, Size>> {

  auto operator()(const sbx::utility::basic_string_literal<Character, Size>& literal) const noexcept -> std::size_t {
    return sbx::utility::fnv1a_hash<Character, std::size_t>{}(literal._data);
  }

}; // struct std::hash

#endif // LIBSBX_UTILITY_STRING_LITERAL_HPP_
