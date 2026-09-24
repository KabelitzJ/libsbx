// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_STATIC_STRING_HPP_
#define LIBSBX_UTILITY_STATIC_STRING_HPP_

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <libsbx/utility/hash.hpp>

#include <fmt/format.h>

namespace sbx::utility {

/**
 * @brief A fixed-capacity, null-terminated string on inline storage — no heap allocation.
 *
 * @tparam Character The character type.
 * @tparam Size The maximum number of characters the string can hold, excluding the
 * implicit null terminator.
 */
template<character Character, std::size_t Size>
class basic_static_string {

  using traits_type = std::char_traits<Character>;

public:

  using value_type = Character;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = value_type*;
  using const_iterator = const value_type*;
  using view_type = std::basic_string_view<value_type>;

  static constexpr auto nposition = static_cast<size_type>(-1);

  constexpr basic_static_string() noexcept
  : _data{}, _size{0} { }

  /**
   * @brief Constructs from a string view.
   *
   * @param string The characters to copy in.
   *
   * @throws std::length_error If string.size() exceeds Size.
   */
  constexpr basic_static_string(view_type string) {
    assign(string);
  }

  /**
   * @brief Constructs from a null-terminated string.
   *
   * @param string The characters to copy in.
   *
   * @throws std::length_error If the string's length exceeds Size.
   */
  constexpr basic_static_string(const value_type* string) {
    assign(view_type{string});
  }

  /**
   * @brief Constructs a string of count repetitions of character.
   *
   * @param count The number of characters to fill.
   * @param character The character to repeat.
   *
   * @throws std::length_error If count exceeds Size.
   */
  constexpr basic_static_string(size_type count, value_type character) {
    if (count > Size) {
      throw std::length_error{"static_string overflow"};
    }

    traits_type::assign(_data.data(), count, character);

    _size = count;
    _data[_size] = value_type{};
  }

  /**
   * @brief Builds a string from an fmt format string and arguments.
   *
   * @tparam Args The types of the format arguments.
   *
   * @param format The fmt format string.
   * @param args The format arguments.
   *
   * @return The formatted string.
   *
   * @note Unlike assign()/append(), this does not throw on overflow — the formatted output is silently truncated to Size characters.
   */
  template<typename... Args>
  requires (std::same_as<value_type, char>)
  static auto format(fmt::format_string<Args...> format, Args&&... args) -> basic_static_string {
    auto result = basic_static_string{};

    auto format_result = fmt::format_to_n(result._data.data(), Size, format, std::forward<Args>(args)...);

    result._size = std::min(static_cast<size_type>(format_result.size), Size);
    result._data[result._size] = value_type{};

    return result;
  }

  /**
   * @brief Replaces the contents with string.
   *
   * @param string The characters to copy in.
   *
   * @return *this.
   *
   * @throws std::length_error If string.size() exceeds Size.
   */
  constexpr auto assign(view_type string) -> basic_static_string& {
    if (string.size() > Size) {
      throw std::length_error{"static_string overflow"};
    }

    traits_type::copy(_data.data(), string.data(), string.size());

    _size = string.size();
    _data[_size] = value_type{};

    return *this;
  }

  constexpr auto operator=(view_type string) -> basic_static_string& {
    return assign(string);
  }

  constexpr auto operator=(const value_type* s) -> basic_static_string& {
    return assign(view_type{s});
  }

  constexpr auto size() const noexcept -> size_type {
    return _size;
  }

  constexpr auto length() const noexcept -> size_type {
    return _size;
  }

  static constexpr auto capacity() noexcept -> size_type {
    return Size;
  }

  static constexpr auto max_size() noexcept -> size_type {
    return Size;
  }

  constexpr auto empty() const noexcept -> bool {
    return _size == 0;
  }

  constexpr auto full() const noexcept -> bool {
    return _size == Size;
  }

  constexpr auto data() noexcept -> value_type* {
    return _data.data();
  }

  constexpr auto data() const noexcept -> const value_type* {
    return _data.data();
  }

  constexpr auto c_str() const noexcept -> const value_type* {
    return _data.data();
  }

  constexpr auto view() const noexcept -> view_type {
    return view_type{_data.data(), _size};
  }

  constexpr operator view_type() const noexcept {
    return view_type{_data.data(), _size};
  }

  /**
   * @brief Unchecked element access.
   *
   * @param index The index to access.
   *
   * @return A reference to the character at index.
   *
   * @warning No bounds checking — index >= size() is undefined behavior. Use at() for a checked lookup.
   */
  constexpr auto operator[](size_type index) noexcept -> reference {
    return _data[index];
  }

  constexpr auto operator[](size_type index) const noexcept -> const_reference {
    return _data[index];
  }

  /**
   * @brief Bounds-checked element access.
   *
   * @param index The index to access.
   *
   * @return A reference to the character at index.
   *
   * @throws std::out_of_range If index >= size().
   */
  constexpr auto at(size_type index) -> reference {
    if (index >= _size) {
      throw std::out_of_range{"static_string::at"};
    }
    return _data[index];
  }

  constexpr auto at(size_type index) const -> const_reference {
    if (index >= _size) {
      throw std::out_of_range{"static_string::at"};
    }

    return _data[index];
  }

  constexpr auto front() noexcept -> reference {
    return _data[0];
  }

  constexpr auto front() const noexcept -> const_reference {
    return _data[0];
  }

  constexpr auto back() noexcept -> reference {
    return _data[_size - 1];
  }

  constexpr auto back() const noexcept -> const_reference {
    return _data[_size - 1];
  }

  constexpr auto begin() noexcept -> iterator {
    return _data.data();
  }

  constexpr auto begin() const noexcept -> const_iterator {
    return _data.data();
  }

  constexpr auto end() noexcept -> iterator {
    return _data.data() + _size;
  }

  constexpr auto end() const noexcept -> const_iterator {
    return _data.data() + _size;
  }

  constexpr auto cbegin() const noexcept -> const_iterator {
    return begin();
  }

  constexpr auto cend() const noexcept -> const_iterator {
    return end();
  }

  constexpr auto clear() noexcept -> void {
    _size = 0;
    _data[0] = value_type{};
  }

  /**
   * @brief Appends a single character.
   *
   * @param character The character to append.
   *
   * @throws std::length_error If the string is already at capacity().
   */
  constexpr auto push_back(value_type character) -> void {
    if (_size >= Size) {
      throw std::length_error{"static_string overflow"};
    }

    _data[_size++] = character;
    _data[_size] = value_type{};
  }

  constexpr auto pop_back() noexcept -> void {
    if (_size > 0) {
      --_size;
      _data[_size] = value_type{};
    }
  }

  /**
   * @brief Appends string to the end of the string.
   *
   * @param string The characters to append.
   *
   * @return *this.
   *
   * @throws std::length_error If the result would exceed Size.
   */
  constexpr auto append(view_type string) -> basic_static_string& {
    if (_size + string.size() > Size) {
      throw std::length_error{"static_string overflow"};
    }

    traits_type::copy(_data.data() + _size, string.data(), string.size());

    _size += string.size();
    _data[_size] = value_type{};
    return *this;
  }

  constexpr auto operator+=(view_type string) -> basic_static_string& {
    return append(string);
  }

  constexpr auto operator+=(value_type character) -> basic_static_string& {
    push_back(character);
    return *this;
  }

  /**
   * @brief Grows or shrinks the string to size, padding new characters with character.
   *
   * @param size The new size.
   * @param character The value used to fill any newly added characters.
   *
   * @throws std::length_error If size exceeds Size.
   */
  constexpr auto resize(size_type size, value_type character = value_type{}) -> void {
    if (size > Size) {
      throw std::length_error{"static_string overflow"};
    }

    if (size > _size) {
      traits_type::assign(_data.data() + _size, size - _size, character);
    }

    _size = size;
    _data[_size] = value_type{};
  }

  /**
   * @brief Returns a view onto a subrange of the string.
   *
   * @param position The index to start the subrange at.
   * @param count The number of characters to include; clamped to the remaining length.
   *
   * @return A view of [position, position + count).
   *
   * @throws std::out_of_range If position > size().
   */
  constexpr auto substr(size_type position = 0, size_type count = nposition) const -> view_type {
    if (position > _size) {
      throw std::out_of_range{"static_string::substr"};
    }

    return view_type{_data.data() + position, std::min(count, _size - position)};
  }

  constexpr auto find(view_type string, size_type position = 0) const noexcept -> size_type {
    return view().find(string, position);
  }

  constexpr auto find(value_type character, size_type position = 0) const noexcept -> size_type {
    return view().find(character, position);
  }

  constexpr auto starts_with(view_type string) const noexcept -> bool {
    return view().starts_with(string);
  }

  constexpr auto ends_with(view_type string) const noexcept -> bool {
    return view().ends_with(string);
  }

  friend constexpr auto operator==(const basic_static_string& a, const basic_static_string& b) noexcept -> bool {
    return a.view() == b.view();
  }

  friend constexpr auto operator<=>(const basic_static_string& a, const basic_static_string& b) noexcept {
    return a.view() <=> b.view();
  }

  friend constexpr auto operator==(const basic_static_string& a, view_type b) noexcept -> bool {
    return a.view() == b;
  }

  friend constexpr auto operator<=>(const basic_static_string& a, view_type b) noexcept {
    return a.view() <=> b;
  }

 private:

  std::array<value_type, Size + 1> _data;
  size_type _size;

}; // class basic_static_string

template <std::size_t Size>
using static_string = basic_static_string<char, Size>;

template <std::size_t Size>
using static_wstring = basic_static_string<wchar_t, Size>;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_STATIC_STRING_HPP_
