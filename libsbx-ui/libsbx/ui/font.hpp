// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_FONT_HPP_
#define LIBSBX_UI_FONT_HPP_

#include <filesystem>
#include <unordered_map>

#include <libsbx/utility/primitive.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/ui/atlas.hpp>

namespace sbx::ui {

using pixels = utility::primitive<std::uint32_t, "px">;

class font {

public:

  struct glyph_info {
    math::vector2u size;
    math::vector2u bearing;
    std::uint32_t advance;
    math::vector2f uv_position;
    math::vector2f uv_size;
  }; // struct glyph_info

  font(const std::filesystem::path& path, pixels height = pixels{32u});

  ~font();

  auto glyph(char character) const noexcept -> const glyph_info&;

  auto atlas() const noexcept -> const ui::atlas&;

private:

  std::unordered_map<char, glyph_info> _glyphs;
  std::unique_ptr<ui::atlas> _atlas;

}; // class font

} // namespace sbx::ui

#endif // LIBSBX_UI_FONT_HPP_
