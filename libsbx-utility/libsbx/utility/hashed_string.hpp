// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_HASHED_STRING_HPP_
#define LIBSBX_UTILITY_HASHED_STRING_HPP_

#include <concepts>
#include <cinttypes>
#include <string>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/target.hpp>

#include <fmt/format.h>

namespace sbx::utility {

template<character Char, typename Hash = std::uint64_t, typename HashFunction = fnv1a_hash<Char, Hash>>
class basic_hashed_string {

public:

  using char_type = HashFunction::char_type;
  using size_type = HashFunction::size_type;
  using hash_type = HashFunction::hash_type;

  inline static constexpr auto npos = std::basic_string<char_type>::npos;

  constexpr basic_hashed_string()
  : _string{},
    _hash{} {}

  constexpr basic_hashed_string(const char_type* string, const size_type length)
  : _string{string, length},
    _hash{HashFunction{}(string)} {}

  template<std::size_t Size>
  constexpr basic_hashed_string(const char_type (&string)[Size])
  : _string{string, Size - 1},
    _hash{HashFunction{}(string)} {}

  constexpr basic_hashed_string(const std::basic_string<char_type>& string)
  : _string{string},
    _hash{HashFunction{}(string)} {}

  constexpr basic_hashed_string(const basic_hashed_string& other) = default;

  constexpr basic_hashed_string(basic_hashed_string&& other) noexcept = default;

  constexpr ~basic_hashed_string() = default;

  constexpr auto operator=(const basic_hashed_string& other) -> basic_hashed_string& = default;

  constexpr auto operator=(basic_hashed_string&& other) noexcept -> basic_hashed_string& = default;

  template<typename Format, typename... Args>
  static auto format(Format&& format, Args&&... args) -> basic_hashed_string {
    return basic_hashed_string{fmt::format(std::forward<Format>(format), std::forward<Args>(args)...)};
  }

  constexpr auto operator==(const basic_hashed_string& other) const noexcept -> bool {
    return _hash == other._hash;
  }

  constexpr auto data() const noexcept -> const char_type* {
    return _string.data();
  }

  constexpr auto size() const noexcept -> size_type {
    return _string.size();
  }

  constexpr auto hash() const noexcept -> hash_type {
    return _hash;
  }

  constexpr auto c_str() const noexcept -> const char_type* {
    return _string.c_str();
  }

  constexpr auto is_empty() const noexcept -> bool {
    return _string.empty();
  }

  constexpr auto str() const noexcept -> const std::basic_string<char_type>& {
    if constexpr (is_build_configuration_debug_v) {
      return _string;
    } else {
      return _string.str();
    }
  }

  constexpr auto rfind(std::basic_string_view<char_type> string) const noexcept -> size_type {
    return _string.rfind(string);
  }

  constexpr auto substr(const size_type position = 0, const size_type count = npos) const -> std::basic_string<char_type> {
    return _string.substr(position, count);
  }

  constexpr operator hash_type() const noexcept {
    return _hash;
  }

private:

  static constexpr auto _empty_cstr() noexcept -> const char_type* {
    static constexpr auto null_char = char_type{0};
    return &null_char;
  }

  struct empty_string {

    template<typename... Args>
    empty_string([[maybe_unused]] Args&&... args) {

    }

     constexpr auto data() const noexcept -> const char_type* {
      return _empty_cstr();
    }

    constexpr auto size() const noexcept -> size_type {
      return 0u;
    }

    constexpr auto c_str() const noexcept -> const char_type* {
      return _empty_cstr();
    }

    constexpr auto empty() const noexcept -> bool {
      return true;
    }

    constexpr auto str() const noexcept -> const std::basic_string<char_type>& {
      static const auto empty = std::basic_string<char_type>{};

      return empty;
    }

    constexpr auto rfind([[maybe_unused]] std::basic_string_view<char_type> string) const noexcept -> size_type {
      return npos;
    }


    constexpr auto substr([[maybe_unused]] const size_type position, [[maybe_unused]] const size_type count) const -> std::basic_string<char_type> {
      return {};
    }

  }; // struct empty_string

  using string_type = std::conditional_t<is_build_configuration_debug_v, std::basic_string<char_type>, empty_string>;

  [[no_unique_address]] string_type _string{};
  hash_type _hash{};

}; // class basic_hashed_string

template<character Char, typename Hash = std::uint64_t, typename HashFunction = fnv1a_hash<Char, Hash>>
constexpr auto operator==(const basic_hashed_string<Char, Hash, HashFunction>& lhs, const basic_hashed_string<Char, Hash, HashFunction>& rhs) noexcept -> bool {
  return lhs.hash() == rhs.hash();
}

using hashed_string = basic_hashed_string<char>;

using hashed_wstring = basic_hashed_string<wchar_t>;

namespace literals {

inline constexpr auto operator""_hs(const char* string, const std::size_t length) -> hashed_string {
  return hashed_string{string, length};
}

inline constexpr auto operator""_hs(const wchar_t* string, const std::size_t length) -> hashed_wstring {
  return hashed_wstring{string, length};
}

} // namespace literals

} // namespace sbx::utility

template<sbx::utility::character Char, typename Hash, typename HashFunction>
struct fmt::formatter<sbx::utility::basic_hashed_string<Char, Hash, HashFunction>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::utility::basic_hashed_string<Char, Hash, HashFunction>& string, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", string.c_str());
  }

}; // struct fmt::formatter

template<sbx::utility::character Char, typename Hash, typename HashFunction>
struct std::hash<sbx::utility::basic_hashed_string<Char, Hash, HashFunction>> {
  auto operator()(const sbx::utility::basic_hashed_string<Char, Hash, HashFunction>& string) const noexcept -> std::size_t {
    return string.hash();
  }
}; // struct std::hash

#endif // LIBSBX_UTILITY_HASHED_STRING_HPP_
