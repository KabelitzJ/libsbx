// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/color.hpp
 * 
 * @brief RGBA color representation and utilities.
 *
 * @details
 * 
 * This header defines a lightweight RGBA color class using floating-point
 * components in the range [0, 1]. The type is intended for rendering,
 * serialization, hashing, and general math usage.
 *
 * The color is stored in linear space and provides convenience constructors,
 * named color factories, component accessors, and interoperability with YAML
 * and hashing utilities.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2023-02-17
 */

#ifndef LIBSBX_MATH_COLOR_HPP_
#define LIBSBX_MATH_COLOR_HPP_

#include <cmath>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/hash.hpp>

namespace sbx::math {

/**
 * @brief RGBA color value type.
 *
 * @details
 * 
 * Represents a color using four floating-point components: red, green, blue,
 * and alpha. Each component is expected to be in the range [0, 1].
 *
 * The class is trivially copyable and designed for use in rendering pipelines,
 * configuration files, and hashing contexts.
 */
class color {

public:

  /**
   * @brief Constructs a white color with full opacity.
   */
  color() noexcept;

  /**
   * @brief Constructs a color from a packed 32-bit RGBA value.
   *
   * @param rgba Packed RGBA value in 0xRRGGBBAA layout.
   */
  color(std::uint32_t rgba) noexcept;

  /**
   * @brief Constructs a color from individual components.
   *
   * @param red   Red component.
   * @param green Green component.
   * @param blue  Blue component.
   * @param alpha Alpha component.
   */
  color(
    std::float_t red,
    std::float_t green,
    std::float_t blue,
    std::float_t alpha = 1.0f
  ) noexcept;

  /**
   * @brief Returns a black color.
   *
   * @return Black color (0, 0, 0, 1).
   */
  static auto black() noexcept -> color;

  /**
   * @brief Returns a white color.
   *
   * @return White color (1, 1, 1, 1).
   */
  static auto white() noexcept -> color;

  /**
   * @brief Returns a red color.
   *
   * @return Red color (1, 0, 0, 1).
   */
  static auto red() noexcept -> color;

  /**
   * @brief Returns a green color.
   *
   * @return Green color (0, 1, 0, 1).
   */
  static auto green() noexcept -> color;

  /**
   * @brief Returns a blue color.
   *
   * @return Blue color (0, 0, 1, 1).
   */
  static auto blue() noexcept -> color;

  /**
   * @brief Returns a magenta color.
   *
   * @return Magenta color (1, 0, 1, 1).
   */
  static auto magenta() noexcept -> color;

  /**
   * @brief Returns a yellow color.
   *
   * @return Yellow color (1, 1, 0, 1).
   */
  static auto yellow() noexcept -> color;

  /**
   * @brief Returns a cyan color.
   *
   * @return Cyan color (0, 1, 1, 1).
   */
  static auto cyan() noexcept -> color;

  /**
   * @brief Returns an orange color.
   *
   * @return Orange color (1, 0.5, 0, 1).
   */
  static auto orange() noexcept -> color;

  /**
   * @brief Returns the red component (const).
   *
   * @return Reference to the red component.
   */
  auto r() const noexcept -> const std::float_t&;

  /**
   * @brief Returns the red component.
   *
   * @return Reference to the red component.
   */
  auto r() noexcept -> std::float_t&;

  /**
   * @brief Returns the green component (const).
   *
   * @return Reference to the green component.
   */
  auto g() const noexcept -> const std::float_t&;

  /**
   * @brief Returns the green component.
   *
   * @return Reference to the green component.
   */
  auto g() noexcept -> std::float_t&;

  /**
   * @brief Returns the blue component (const).
   *
   * @return Reference to the blue component.
   */
  auto b() const noexcept -> const std::float_t&;

  /**
   * @brief Returns the blue component.
   *
   * @return Reference to the blue component.
   */
  auto b() noexcept -> std::float_t&;

  /**
   * @brief Returns the alpha component (const).
   *
   * @return Reference to the alpha component.
   */
  auto a() const noexcept -> const std::float_t&;

  /**
   * @brief Returns the alpha component.
   *
   * @return Reference to the alpha component.
   */
  auto a() noexcept -> std::float_t&;

private:

  std::float_t _red;
  std::float_t _green;
  std::float_t _blue;
  std::float_t _alpha;

}; // class color

/**
 * @brief Equality comparison for colors.
 *
 * @param lhs Left-hand side color.
 * @param rhs Right-hand side color.
 *
 * @return True if all components are equal.
 */
auto operator==(const color& lhs, const color& rhs) noexcept -> bool;

/**
 * @brief Scales a color by a scalar factor.
 *
 * @param lhs   Color to scale.
 * @param value Scale factor.
 *
 * @return Scaled color.
 */
auto operator*(color lhs, const std::float_t value) -> color;

} // namespace sbx::math

/**
 * @brief YAML conversion specialization for color.
 */
template<>
struct YAML::convert<sbx::math::color> {

  static auto encode(const sbx::math::color& color) -> Node;
  static auto decode(const Node& node, sbx::math::color& color) -> bool;

}; // struct YAML::convert

/**
 * @brief YAML stream output for color.
 */
auto operator<<(YAML::Emitter& out, const sbx::math::color& color) -> YAML::Emitter&;

/**
 * @brief Hash specialization for color.
 */
template<>
struct std::hash<sbx::math::color> {

  auto operator()(const sbx::math::color& color) const noexcept -> std::size_t;

}; // struct std::hash

#endif // LIBSBX_MATH_COLOR_HPP_
