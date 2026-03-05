// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_FONT_HPP_
#define LIBSBX_UI_FONT_HPP_

#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/images/image.hpp>

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

  [[nodiscard]] auto find_glyph(std::uint32_t codepoint) const -> const glyph* {
    auto it = glyphs.find(codepoint);

    if (it == glyphs.end()) {
      return nullptr;
    }

    return &it->second;
  }
}; // struct font

[[nodiscard]] inline auto load_font(graphics::image2d_handle atlas, const std::filesystem::path& json_path) -> font {
  auto file = std::ifstream{json_path};
  auto json = nlohmann::json::parse(file);

  auto result = font{};
  result.atlas = atlas;

  const auto& atlas_info = json["atlas"];
  result.atlas_width = atlas_info["width"].get<std::float_t>();
  result.atlas_height = atlas_info["height"].get<std::float_t>();
  result.sdf_px_range = atlas_info["distanceRange"].get<std::float_t>();

  const auto& metrics = json["metrics"];
  result.line_height = metrics["lineHeight"].get<std::float_t>();

  for (const auto& g : json["glyphs"]) {
    auto glyph_data = glyph{};

    glyph_data.advance = g["advance"].get<std::float_t>();

    if (g.contains("planeBounds")) {
      const auto& pb = g["planeBounds"];
      glyph_data.plane_left = pb["left"].get<std::float_t>();
      glyph_data.plane_bottom = pb["bottom"].get<std::float_t>();
      glyph_data.plane_right = pb["right"].get<std::float_t>();
      glyph_data.plane_top = pb["top"].get<std::float_t>();
    }

    if (g.contains("atlasBounds")) {
      const auto& ab = g["atlasBounds"];
      glyph_data.uv_left = ab["left"].get<std::float_t>() / result.atlas_width;
      glyph_data.uv_bottom = ab["bottom"].get<std::float_t>() / result.atlas_height;
      glyph_data.uv_right = ab["right"].get<std::float_t>() / result.atlas_width;
      glyph_data.uv_top = ab["top"].get<std::float_t>() / result.atlas_height;
    }

    const auto codepoint = g["unicode"].get<std::uint32_t>();
    result.glyphs[codepoint] = glyph_data;
  }

  return result;
}

} // namespace sbx::ui

#endif // LIBSBX_UI_FONT_HPP_
