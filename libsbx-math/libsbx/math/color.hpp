// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_COLOR_HPP_
#define LIBSBX_MATH_COLOR_HPP_

#include <cmath>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/hash.hpp>

// #include <libsbx/math/vector4.hpp>

namespace sbx::math {

class color {

public:

  color() noexcept;

  color(std::uint32_t rgba) noexcept;

  color(std::float_t red, std::float_t green, std::float_t blue, std::float_t alpha = 1.0f) noexcept;

  static auto black() noexcept -> color {
    return color{0.0f, 0.0f, 0.0f, 1.0f};
  }

  static auto white() noexcept -> color {
    return color{1.0f, 1.0f, 1.0f, 1.0f};
  }

  static auto red() noexcept -> color {
    return color{1.0f, 0.0f, 0.0f, 1.0f};
  }

  static auto green() noexcept -> color {
    return color{0.0f, 1.0f, 0.0f, 1.0f};
  }

  static auto blue() noexcept -> color {
    return color{0.0f, 0.0f, 1.0f, 1.0f};
  }

  static auto magenta() noexcept -> color {
    return color{1.0f, 0.0f, 1.0f, 1.0f};
  }

  static auto yellow() noexcept -> color {
    return color{1.0f, 1.0f, 0.0f, 1.0f};
  }

  static auto cyan() noexcept -> color {
    return color{0.0f, 1.0f, 1.0f, 1.0f};
  }

  static auto orange() noexcept -> color {
    return color{1.0f, 0.5f, 0.0f, 1.0f};
  }

  auto r() const noexcept -> const std::float_t&;

  auto r() noexcept -> std::float_t&;

  auto g() const noexcept -> const std::float_t&;

  auto g() noexcept -> std::float_t&;

  auto b() const noexcept -> const std::float_t&;

  auto b() noexcept -> std::float_t&;

  auto a() const noexcept -> const std::float_t&;

  auto a() noexcept -> std::float_t&;

private:

  std::float_t _red;
  std::float_t _green;
  std::float_t _blue;
  std::float_t _alpha;

}; // class color

auto operator==(const color& lhs, const color& rhs) noexcept -> bool;

auto operator*(color lhs, const std::float_t value) -> color;

} // namespace sbx::math

template<>
struct YAML::convert<sbx::math::color> {
  static auto encode(const sbx::math::color& color) -> Node;
  static auto decode(const Node& node, sbx::math::color& color) -> bool;
}; // struct YAML::convert

auto operator<<(YAML::Emitter& out, const sbx::math::color& color) -> YAML::Emitter&;

template<>
struct std::hash<sbx::math::color> {
  auto operator()(const sbx::math::color& color) const noexcept -> std::size_t;
}; // struct std::hash

#endif // LIBSBX_MATH_COLOR_HPP_
