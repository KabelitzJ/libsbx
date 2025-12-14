#ifndef LIBSBX_GRAPHICS_VIEWPORT_HPP_
#define LIBSBX_GRAPHICS_VIEWPORT_HPP_

#include <optional>

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector2.hpp>

namespace sbx::graphics {

class viewport {
  
public:

  enum class type : std::uint8_t {
    fixed = utility::bit_v<0>,
    window = utility::bit_v<1>,
    dynamic = utility::bit_v<2>,
    all = fixed | window | dynamic
  }; // enum class type

  static auto fixed(const math::vector2u& size) -> viewport {
    return viewport{type::fixed, math::vector2f{1.0f, 1.0f}, math::vector2i{0, 0}, size};
  }

  static auto fixed(const std::uint32_t width, const std::uint32_t height) -> viewport {
    return viewport{type::fixed, math::vector2f{1.0f, 1.0f}, math::vector2i{0, 0}, math::vector2u{width, height}};
  }

  static auto window(const math::vector2f& scale = math::vector2f{1.0f, 1.0f}) -> viewport {
    return viewport{type::window, scale, math::vector2i{0, 0}, std::nullopt};
  }

  static auto dynamic(const math::vector2f& scale = math::vector2f{1.0f, 1.0f}) -> viewport {
    return viewport{type::dynamic, scale, math::vector2i{0, 0}, std::nullopt};
  }

  auto scale() const noexcept -> const math::vector2f& {
    return _scale;
  }

  auto set_scale(const math::vector2f& scale) noexcept -> void {
    _scale = scale;
  }

  auto offset() const noexcept -> const math::vector2i& {
    return _offset;
  }

  auto set_offset(const math::vector2i& offset) noexcept -> void {
    _offset = offset;
  }

  auto size() const noexcept -> const std::optional<math::vector2u>& {
    return _size;
  }

  auto set_size(const math::vector2u& size) noexcept -> void {
    _size = size;
  }

  auto is_fixed() const noexcept -> bool {
    return _type == type::fixed;
  }

  auto is_window() const noexcept -> bool {
    return _type == type::window;
  }

  auto is_dynamic() const noexcept -> bool {
    return _type == type::dynamic;
  }

  auto is_type(const type flags) const noexcept -> bool {
    return utility::to_underlying(flags) & utility::to_underlying(_type);
  }

private:

  viewport(const type type, const math::vector2f& scale, const math::vector2i& offset, const std::optional<math::vector2u>& size = std::nullopt) noexcept
  : _type{type},
    _scale{scale}, 
    _offset{offset}, 
    _size{size} { }

  type _type;
  math::vector2f _scale;
  math::vector2i _offset;
  std::optional<math::vector2u> _size;

}; // class viewport

inline constexpr auto operator|(const viewport::type lhs, const viewport::type rhs) noexcept -> viewport::type {
  return utility::from_underlying<viewport::type>(utility::to_underlying(lhs) | utility::to_underlying(rhs));
}

class render_area {

public:

  render_area(const math::vector2u& extent = math::vector2u{}, const math::vector2i& offset = math::vector2i{}) noexcept
  : _extent{extent}, 
    _offset{offset}, 
    _aspect_ratio{static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())} { }

  auto operator==(const render_area& other) const noexcept -> bool {
    return _extent == other._extent && _offset == other._offset;
  }

  auto extent() const noexcept -> const math::vector2u& {
    return _extent;
  }

  auto set_extent(const math::vector2u& extent) noexcept -> void {
    _extent = extent;
  }

  auto offset() const noexcept -> const math::vector2i& {
    return _offset;
  }

  auto set_offset(const math::vector2i& offset) noexcept -> void {
    _offset = offset;
  }

  auto aspect_ratio() const noexcept -> std::float_t {
    return _aspect_ratio;
  }

  auto set_aspect_ratio(std::float_t aspect_ratio) noexcept -> void {
    _aspect_ratio = aspect_ratio;
  }

private:

  math::vector2u _extent;
  math::vector2i _offset;
  std::float_t _aspect_ratio;

}; // class render_area

}; // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_VIEWPORT_HPP_
