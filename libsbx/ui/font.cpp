// SPDX-License-Identifier: MIT
#include <libsbx/ui/font.hpp>

namespace sbx::ui {

[[nodiscard]] auto font::find_glyph(std::uint32_t codepoint) const -> memory::observer_ptr<const glyph> {
  auto entry = glyphs.find(codepoint);

  if (entry == glyphs.end()) {
    return nullptr;
  }

  return &entry->second;
}

[[nodiscard]] auto load_font(graphics::image2d_handle atlas, const std::filesystem::path& json_path) -> font {
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
