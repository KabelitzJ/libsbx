// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_ALIAS_HPP_
#define LIBSBX_FILESYSTEM_ALIAS_HPP_

#include <string>
#include <string_view>

#include <libsbx/utility/hash.hpp>

namespace sbx::filesystem {

template<utility::character Char>
class basic_alias {
public:

  using char_type = Char;
  using string_type = std::basic_string<char_type>;
  using string_view_type = std::basic_string_view<char_type>;
  using size_type = std::size_t;
  using hash_type = std::size_t;

  basic_alias()
  : _value{_normalize(string_view_type{})} { }

  explicit basic_alias(string_view_type alias)
  : _value{_normalize(alias)} { }

  static auto root() -> basic_alias {
    return basic_alias{string_view_type{}};
  }

  auto string() const & noexcept -> const string_type& {
    return _value;
  }

  auto string() && noexcept -> string_type {
    return std::move(_value);
  }

  auto view() const noexcept -> string_view_type {
    return _value;
  }

  auto length() const noexcept -> size_type {
    return _value.length();
  }

  auto hash() const noexcept -> hash_type {
    return static_cast<hash_type>(utility::fnv1a_hash<char_type, std::uint64_t>{}(_value));
  }

  auto operator==(const basic_alias& other) const noexcept -> bool {
    return _value == other._value;
  }

private:

  static auto _normalize(string_view_type alias) -> string_type {
    auto normalized = string_type{alias};
    auto whitespace = string_view_type{reinterpret_cast<const char_type*>(" \t\n\r")};

    auto begin = normalized.find_first_not_of(whitespace);

    if (begin == string_type::npos) {
      normalized.clear();
    } else if (begin > 0) {
      normalized.erase(0, begin);
    }

    if (!normalized.empty()) {
      auto end = normalized.find_last_not_of(whitespace);

      if (end != string_type::npos && end + 1 < normalized.size()) {
        normalized.erase(end + 1);
      }
    }

    if (normalized.empty()) {
      normalized = reinterpret_cast<const char_type*>("/");
    }

    if (normalized.front() != static_cast<char_type>('/')) {
      normalized.insert(normalized.begin(), static_cast<char_type>('/'));
    }

    while (normalized.size() > 1 && normalized.back() == static_cast<char_type>('/')) {
      normalized.pop_back();
    }

    if (normalized.back() != static_cast<char_type>('/')) {
      normalized.push_back(static_cast<char_type>('/'));
    }

    return normalized;
  }

  string_type _value;

}; // class basic_alias

using alias = basic_alias<char>;

} // namespace sbx::filesystem

template<sbx::utility::character Char>
struct std::hash<sbx::filesystem::basic_alias<Char>> {

  auto operator()(const sbx::filesystem::basic_alias<Char>& alias) const noexcept -> std::size_t {
    return alias.hash();
  }

}; // struct std::hash<sbx::filesystem::basic_alias<Char>>

#endif // LIBSBX_FILESYSTEM_ALIAS_HPP_
