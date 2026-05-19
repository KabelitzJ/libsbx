// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_UUID_HPP_
#define LIBSBX_MATH_UUID_HPP_

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

#endif // LIBSBX_MATH_UUID_HPP_

