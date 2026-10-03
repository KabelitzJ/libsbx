// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_FONT_HPP_
#define LIBSBX_ASSETS_FONT_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/texture2d.hpp>

namespace sbx::assets {

class font final : public loadable {

  friend class asset_residency;

public:

  struct glyph {
    math::vector4 uv_rect{0.0f, 0.0f, 0.0f, 0.0f};
    std::float_t width{0.0f};
    std::float_t height{0.0f};
    std::float_t bearing_x{0.0f};
    std::float_t bearing_y{0.0f};
    std::float_t advance{0.0f};
  }; // struct glyph

  font() = default;

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _atlas.is_valid() && _atlas->is_valid();
  }

  [[nodiscard]] auto atlas() const noexcept -> const texture2d_handle& {
    return _atlas;
  }

  [[nodiscard]] auto glyph_for(std::uint32_t codepoint) const noexcept -> memory::observer_ptr<const glyph> {
    if (codepoint < _first_codepoint) {
      return nullptr;
    }

    const auto index = static_cast<std::size_t>(codepoint - _first_codepoint);

    if (index >= _glyphs.size()) {
      return nullptr;
    }

    return memory::make_observer<const glyph>(_glyphs[index]);
  }

  [[nodiscard]] auto line_height() const noexcept -> std::float_t {
    return _line_height;
  }

  [[nodiscard]] auto ascent() const noexcept -> std::float_t {
    return _ascent;
  }

  [[nodiscard]] auto descent() const noexcept -> std::float_t {
    return _descent;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  // Fills a placeholder font once its cooked content arrives (main thread). Bumps generation() so caches keyed on the font notice the real glyphs.
  auto _finalize_content(std::vector<glyph> glyphs, std::uint32_t first_codepoint, std::float_t line_height, std::float_t ascent, std::float_t descent) -> void {
    _glyphs = std::move(glyphs);
    _first_codepoint = first_codepoint;
    _line_height = line_height;
    _ascent = ascent;
    _descent = descent;
    _bump_generation();
  }

  texture2d_handle _atlas{};
  std::vector<glyph> _glyphs{};
  std::uint32_t _first_codepoint{0u};
  std::float_t _line_height{0.0f};
  std::float_t _ascent{0.0f};
  std::float_t _descent{0.0f};
  math::uuid _id{math::uuid::nil()};

}; // class font

using font_handle = asset_handle<font>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_FONT_HPP_
