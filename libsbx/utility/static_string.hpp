// SPDX-License-Identifier: MIT
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

  constexpr basic_static_string(view_type string) {
    assign(string);
  }

  constexpr basic_static_string(const value_type* string) {
    assign(view_type{string});
  }

  constexpr basic_static_string(size_type count, value_type character) {
    if (count > Size) {
      throw std::length_error{"static_string overflow"};
    }

    traits_type::assign(_data.data(), count, character);

    _size = count;
    _data[_size] = value_type{};
  }

  template<typename... Args>
  requires (std::same_as<value_type, char>)
  static auto format(fmt::format_string<Args...> format, Args&&... args) -> basic_static_string {
    auto result = basic_static_string{};
 
    auto format_result = fmt::format_to_n(result._data.data(), Size, format, std::forward<Args>(args)...);
 
    result._size = std::min(static_cast<size_type>(format_result.size), Size);
    result._data[result._size] = value_type{};
 
    return result;
  }

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

  constexpr auto operator[](size_type index) noexcept -> reference {
    return _data[index];
  }

  constexpr auto operator[](size_type index) const noexcept -> const_reference {
    return _data[index];
  }

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