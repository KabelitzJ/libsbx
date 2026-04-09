// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_VIEWPORT_HPP_
#define LIBSBX_GRAPHICS_VIEWPORT_HPP_

#include <optional>
#include <string>
#include <string_view>

#include <libsbx/math/vector2.hpp>

namespace sbx::graphics {

class viewport {

public:

  enum class kind : std::uint8_t {
    fixed,
    named
  }; // enum class kind

  inline static constexpr auto window_name = std::string_view{"window"};

  static auto fixed(const math::vector2u& size) -> viewport {
    return viewport{kind::fixed, std::string{}, math::vector2f{1.0f, 1.0f}, math::vector2i{0, 0}, size};
  }

  static auto fixed(const std::uint32_t width, const std::uint32_t height) -> viewport {
    return fixed(math::vector2u{width, height});
  }

  static auto window(const math::vector2f& scale = math::vector2f{1.0f, 1.0f}) -> viewport {
    return viewport{kind::named, std::string{window_name}, scale, math::vector2i{0, 0}, std::nullopt};
  }

  static auto named(std::string name, const math::vector2f& scale = math::vector2f{1.0f, 1.0f}) -> viewport {
    return viewport{kind::named, std::move(name), scale, math::vector2i{0, 0}, std::nullopt};
  }

  auto scale() const noexcept -> const math::vector2f& {
    return _scale;
  }

  auto offset() const noexcept -> const math::vector2i& {
    return _offset;
  }

  auto size() const noexcept -> const std::optional<math::vector2u>& {
    return _size;
  }

  auto is_fixed() const noexcept -> bool {
    return _kind == kind::fixed;
  }

  auto is_named() const noexcept -> bool {
    return _kind == kind::named;
  }

  auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  viewport(const kind kind, std::string name, const math::vector2f& scale, const math::vector2i& offset, const std::optional<math::vector2u>& size) noexcept
  : _kind{kind},
    _name{std::move(name)},
    _scale{scale},
    _offset{offset},
    _size{size} { }

  kind _kind;
  std::string _name;
  math::vector2f _scale;
  math::vector2i _offset;
  std::optional<math::vector2u> _size;

}; // class viewport

class render_area {

public:

  render_area(const math::vector2u& extent = math::vector2u{}, const math::vector2i& offset = math::vector2i{}) noexcept
  : _extent{extent},
    _offset{offset},
    _aspect_ratio{extent.y() == 0u ? 1.0f : static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())} { }

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

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_VIEWPORT_HPP_
