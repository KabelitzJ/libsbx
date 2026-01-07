#include <libsbx/ui/font.hpp>

#include <cmath>

#include <algorithm>
#include <filesystem>
#include <ranges>

#include <freetype/freetype.h>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/utility/timer.hpp>

#include <libsbx/ui/ui_module.hpp>

namespace sbx::ui {

static constexpr auto num_glyphs = 128u;

font::font(const std::filesystem::path& path, pixels height) {
  auto library = FT_Library{};
  auto face = FT_Face{};

  auto error = FT_Error{0};

  auto timer = utility::timer{};

  error = FT_Init_FreeType(&library);

  if (error) {
    throw std::runtime_error("Failed to initialize FreeType library");
  }

  error = FT_New_Face(library, path.string().c_str(), 0, &face);

  if (error == FT_Err_Unknown_File_Format) {
    throw std::runtime_error{fmt::format("Failed to load font '{}': unknown file format", path.string())};
  } else if (error) {
    throw std::runtime_error{fmt::format("Failed to load font '{}': unknown error", path.string())};
  }

  error = FT_Set_Char_Size(face, 0, height << 6, 96, 96);

  if (error) {
    throw std::runtime_error{fmt::format("Failed to set font size to '{}'", height)};
  }

  auto atlas_size = math::vector2u{0, 0};

  for (const auto character : std::views::iota(0u, num_glyphs)) {
    error = FT_Load_Char(face, character, FT_LOAD_RENDER);

    if (error) {
      throw std::runtime_error{fmt::format("Failed to load glyph '{}'", character)};
    }

    atlas_size.x() += face->glyph->bitmap.width;
    atlas_size.y() = std::max(atlas_size.y(), face->glyph->bitmap.rows);
  }

  auto atlas_data = std::vector<std::uint8_t>{};
  atlas_data.resize(atlas_size.x() * atlas_size.y());

  auto atlas_position = math::vector2u{0, 0};

  for (const auto character : std::views::iota(0u, num_glyphs)) {
    error = FT_Load_Char(face, character, FT_LOAD_RENDER);

    if (error) {
      throw std::runtime_error{fmt::format("Failed to load glyph '{}'", character)};
    }

    const auto glyph = face->glyph;

    for (auto y = 0u; y < glyph->bitmap.rows; ++y) {
      for (auto x = 0u; x < glyph->bitmap.width; ++x) {
        const auto atlas_index = (atlas_position.y() + y) * atlas_size.x() + (atlas_position.x() + x);
        const auto glyph_index = y * glyph->bitmap.width + x;

        atlas_data[atlas_index] = glyph->bitmap.buffer[glyph_index];
      }
    }

    auto glyph_info = font::glyph_info{};

    glyph_info.size = math::vector2u{glyph->bitmap.width, glyph->bitmap.rows};
    glyph_info.bearing = math::vector2u{glyph->bitmap_left, glyph->bitmap_top};
    glyph_info.advance = (glyph->advance.x >> 6);
    glyph_info.uv_position = math::vector2f{static_cast<float>(atlas_position.x()) / atlas_size.x(), static_cast<float>(atlas_position.y()) / atlas_size.y()};
    glyph_info.uv_size = math::vector2f{static_cast<float>(glyph->bitmap.width) / atlas_size.x(), static_cast<float>(glyph->bitmap.rows) / atlas_size.y()};

    _glyphs.insert({character, glyph_info});

    atlas_position.x() += glyph->bitmap.width;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(library);

  _atlas = std::make_unique<ui::atlas>(atlas_size.x(), atlas_size.y(), atlas_data);

  utility::logger<"ui">::debug("Created font atlas for font '{}' in {} ({}x{}) in {:.2f}ms", path.string(), height, atlas_size.x(), atlas_size.y(), units::quantity_cast<units::millisecond>(timer.elapsed()));
}

font::~font() {

}

auto font::glyph(char character) const noexcept -> const glyph_info& {
  return _glyphs.at(character);
}

auto font::atlas() const noexcept -> const ui::atlas& {
  return *_atlas;
}

} // namespace sbx::ui
