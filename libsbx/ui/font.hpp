// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_FONT_HPP_
#define LIBSBX_UI_FONT_HPP_

#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::ui {

struct glyph {
  std::float_t advance{0.0f};

  std::float_t plane_left{0.0f};
  std::float_t plane_bottom{0.0f};
  std::float_t plane_right{0.0f};
  std::float_t plane_top{0.0f};

  std::float_t uv_left{0.0f};
  std::float_t uv_bottom{0.0f};
  std::float_t uv_right{0.0f};
  std::float_t uv_top{0.0f};
}; // struct glyph

struct font {
  graphics::image2d_handle atlas{};
  std::float_t atlas_width{0.0f};
  std::float_t atlas_height{0.0f};
  std::float_t line_height{1.0f};
  std::float_t sdf_px_range{2.0f};
  std::unordered_map<std::uint32_t, glyph> glyphs;

  [[nodiscard]] auto find_glyph(std::uint32_t codepoint) const -> memory::observer_ptr<const glyph>;

}; // struct font

[[nodiscard]] auto load_font(graphics::image2d_handle atlas, const std::filesystem::path& json_path) -> font;

} // namespace sbx::ui

#endif // LIBSBX_UI_FONT_HPP_
