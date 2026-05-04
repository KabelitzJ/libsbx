// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_UUID_HPP_
#define LIBSBX_MATH_UUID_HPP_

#if defined(SBX_MATH_UUID_USE_V4)

#include <cinttypes>
#include <exception>
#include <charconv>

#include <fmt/format.h>

#include <range/v3/all.hpp>

#include <libsbx/utility/hash.hpp>

#include <libsbx/math/random.hpp>

namespace sbx::math {

struct invalid_uuid_exception : std::runtime_error {

  invalid_uuid_exception(std::string_view uuid)
  : std::runtime_error{fmt::format("String '{}' is not a valid uuid", uuid)} { }

}; // struct invalid_uuid_exception

class uuid {

  friend struct fmt::formatter<sbx::math::uuid>;
  friend struct std::hash<sbx::math::uuid>;

  struct null_uui_tag { };

public:

  // static const uuid null;

  using value_type = std::uint64_t;

  uuid()
  : _value{random::next<value_type>()} { 
    // Taken from https://www.cryptosys.net/pki/uuid-rfc4122.html

    // // 1. Generate 16 random bytes (=128 bits)
    // for (auto& byte : _bytes) {
    //   byte = random::next<std::uint8_t>();
    // }

    // 2. Adjust certain bits according to RFC 4122 section 4.4 as follows:
    //   (a) set the four most significant bits of the 7th byte to 0100'B, so the high nibble is "4"
    //   (b) set the two most significant bits of the 9th byte to 10'B, so the high nibble will be one of "8", "9", "A" or "B"
    // _bytes[6] = 0x40 | (_bytes[6] & 0xf);
    // _bytes[8] = 0x80 | (_bytes[8] & 0x3f);
  }

  // uuid(std::string_view string) {
  //   if (string.size() != 36u) {
  //     throw invalid_uuid_exception{string};
  //   }

  //   for (auto i = 0u, offset = 0u; i < _bytes.size(); ++i) {

  //     if (i == 4u || i == 6u || i == 8u || i == 10u) {
  //       ++offset;
  //     }

  //     auto substr = string.substr(i * 2u + offset, 2u);
  //     const auto [itr, error] = std::from_chars(substr.data(), substr.data() + 2u, _bytes[i], 16);

  //     if (error == std::errc::invalid_argument || error == std::errc::result_out_of_range) {
  //       throw invalid_uuid_exception{string};
  //     }
  //   }

  // }

  ~uuid() = default;

  static auto null() -> uuid {
    return uuid{null_uui_tag{}};
  }

  auto operator==(const uuid& other) const noexcept -> bool {
    return _value == other._value;
  }

  // operator std::string() const {
  //   return fmt::format(
  //     "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
  //     _bytes[0], _bytes[1], _bytes[2], _bytes[3], _bytes[4], _bytes[5], _bytes[6], _bytes[7],
  //     _bytes[8], _bytes[9], _bytes[10], _bytes[11], _bytes[12], _bytes[13], _bytes[14], _bytes[15]
  //   );
  // }

  operator value_type() const {
    return _value;
  }

private:

  uuid(null_uui_tag)
  : _value{0} { }

  // std::array<std::uint8_t, 16u> _bytes;
  value_type _value;

}; // class uuid

} // namespace sbx::math

template<>
struct fmt::formatter<sbx::math::uuid> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin()) {
    return context.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::math::uuid& uuid, FormatContext& context) -> decltype(context.out()) {
    return fmt::format_to(context.out(), "{}", static_cast<sbx::math::uuid::value_type>(uuid));
  }
}; // struct fmt::formatter<sbx::math::uuid>

// template<>
// struct YAML::convert<sbx::math::uuid> {

//   static auto encode(const sbx::math::uuid& rhs) -> YAML::Node {
//     return Node{std::string{rhs}};
//   }

//   static auto decode(const YAML::Node& node, sbx::math::uuid& rhs) -> bool {
//     rhs = sbx::math::uuid{node.as<std::string>()};

//     return true;
//   }

// }; // struct YAML::convert<sbx::math::uuid>

template<>
struct std::hash<sbx::math::uuid> {
  auto operator()(const sbx::math::uuid& uuid) const noexcept -> std::size_t {
    return static_cast<sbx::math::uuid::value_type>(uuid);
  }
}; // struct std::hash<sbx::math::uuid>

#else // SBX_MATH_UUID_USE_V4

#include <cinttypes>
#include <concepts>

#include <fmt/format.h>

#include <libsbx/math/random.hpp>

namespace sbx::math {

template<std::unsigned_integral Type>
class basic_uuid {

public:

  using value_type = Type;

  basic_uuid()
  : _value{random::next<value_type>(1u)} { }

  static constexpr auto nil() -> basic_uuid {
    return basic_uuid{0u};
  }

  static constexpr auto from_value(const value_type value) -> basic_uuid {
    return basic_uuid{value};
  }

  static constexpr auto create() -> basic_uuid {
    return basic_uuid{random::next<value_type>()};
  }

  constexpr auto operator==(const basic_uuid& other) const noexcept -> bool {
    return _value == other._value;
  }

  constexpr auto operator<(const basic_uuid& other) const noexcept -> bool {
    return _value < other._value;
  }

  constexpr auto value() const noexcept -> value_type {
    return _value;
  }

private:

  basic_uuid(const value_type value)
  : _value{value} { }

  value_type _value;

}; // class uuid

using uuid = basic_uuid<std::uint64_t>;

} // namespace sbx::math

template<std::unsigned_integral Type>
struct fmt::formatter<sbx::math::basic_uuid<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin()) {
    return context.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::math::basic_uuid<Type>& uuid, FormatContext& context) const -> decltype(context.out()) {
    static constexpr auto width = sizeof(Type) * 2;

    if (uuid == sbx::math::basic_uuid<Type>::nil()) {
      return fmt::format_to(context.out(), "[nil]");
    }

    return fmt::format_to(context.out(), "{:0{}x}", uuid.value(), width);
  }
}; // struct fmt::formatter<sbx::math::uuid>

template<std::unsigned_integral Type>
struct YAML::convert<sbx::math::basic_uuid<Type>> {

  static auto encode(const sbx::math::basic_uuid<Type>& rhs) -> YAML::Node {
    return Node{rhs.value()};
  }

  static auto decode(const YAML::Node& node, sbx::math::basic_uuid<Type>& rhs) -> bool {
    if (!node.IsScalar()) {
      return false;
    }

    rhs = sbx::math::basic_uuid<Type>::from_value(node.as<Type>());

    return true;
  }

}; // struct YAML::convert<sbx::math::basic_vector3<Type>>

template<std::unsigned_integral Type>
auto operator<<(YAML::Emitter& out, const sbx::math::basic_uuid<Type>& vector) -> YAML::Emitter& {
  return out << YAML::convert<sbx::math::basic_uuid<Type>>::encode(vector);
}

template<std::unsigned_integral Type>
struct std::hash<sbx::math::basic_uuid<Type>> {
  auto operator()(const sbx::math::basic_uuid<Type>& uuid) const noexcept -> std::size_t {
    return uuid.value();
  }
}; // struct std::hash<sbx::math::uuid>

#endif // SBX_MATH_UUID_USE_V4

#endif // LIBSBX_MATH_UUID_HPP_

