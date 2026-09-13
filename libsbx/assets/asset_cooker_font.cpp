// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

inline constexpr auto font_magic = utility::fourcc_v<"SBFN">; // 'SBFN'

// The pixel height a font's atlas is rasterized at; every glyph metric is stored normalized by
// this (i.e. per one unit of ui_text::font_size), so one atlas serves any font_size at runtime.
inline constexpr auto font_reference_size = 48.0f;
inline constexpr auto font_sdf_padding = 4;
inline constexpr auto font_sdf_onedge_value = 128u;
inline constexpr auto font_sdf_pixel_dist_scale = 32.0f; // onedge_value / padding
inline constexpr auto font_first_codepoint = std::uint32_t{32u};
inline constexpr auto font_codepoint_count = std::uint32_t{224u}; // 32..255: printable ASCII + Latin-1
struct font_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t atlas_width;
  std::uint32_t atlas_height;
  std::uint32_t glyph_count;
  std::uint32_t first_codepoint;
  std::float_t line_height;
  std::float_t ascent;
  std::float_t descent;
  std::uint32_t atlas_data_size; // bytes of single-channel atlas pixel data following the glyph table
}; // struct font_header

struct font_glyph_record {
  std::float_t uv_x0;
  std::float_t uv_y0;
  std::float_t uv_x1;
  std::float_t uv_y1;
  std::float_t width;
  std::float_t height;
  std::float_t bearing_x;
  std::float_t bearing_y;
  std::float_t advance;
}; // struct font_glyph_record

auto asset_cooker::resolve_font(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<cooked_font_data> {
  did_cook = false;

  if (needs_cook) {
    if (!_cook_font(source, cooked)) {
      utility::logger<"assets">::warn("Could not cook font '{}'", source.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  auto data = cooked_font_data{};

  if (!_load_cooked_font(cooked, data)) {
    if (!_cook_font(source, cooked) || !_load_cooked_font(cooked, data)) {
      utility::logger<"assets">::warn("Could not load cooked font '{}'", cooked.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  return data;
}

auto asset_cooker::_cook_font(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto in = std::ifstream{source, std::ios::binary};

  if (!in) {
    utility::logger<"assets">::warn("Cook: could not open '{}'", source.generic_string());
    return false;
  }

  const auto buffer = std::vector<unsigned char>{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};

  auto font_info = stbtt_fontinfo{};

  if (!stbtt_InitFont(&font_info, buffer.data(), stbtt_GetFontOffsetForIndex(buffer.data(), 0))) {
    utility::logger<"assets">::warn("Cook: could not parse font '{}'", source.generic_string());
    return false;
  }

  const auto scale = stbtt_ScaleForPixelHeight(&font_info, font_reference_size);

  auto ascent_units = std::int32_t{0};
  auto descent_units = std::int32_t{0};
  auto line_gap_units = std::int32_t{0};

  stbtt_GetFontVMetrics(&font_info, &ascent_units, &descent_units, &line_gap_units);

  struct baked_glyph {
    std::vector<std::uint8_t> bitmap{};
    std::int32_t width{0};
    std::int32_t height{0};
    std::int32_t xoff{0};
    std::int32_t yoff{0};
    std::float_t advance{0.0f};
  }; // struct baked_glyph

  auto baked = std::vector<baked_glyph>(font_codepoint_count);

  auto max_width = std::int32_t{1};
  auto max_height = std::int32_t{1};

  for (auto index = std::uint32_t{0u}; index < font_codepoint_count; ++index) {
    const auto codepoint = static_cast<std::int32_t>(font_first_codepoint + index);

    auto advance_units = std::int32_t{0};
    auto left_side_bearing_units = std::int32_t{0};

    stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance_units, &left_side_bearing_units);

    auto& glyph = baked[index];
    glyph.advance = static_cast<std::float_t>(advance_units) * scale;

    auto width = std::int32_t{0};
    auto height = std::int32_t{0};
    auto xoff = std::int32_t{0};
    auto yoff = std::int32_t{0};

    auto* bitmap = stbtt_GetCodepointSDF(&font_info, scale, codepoint, font_sdf_padding, static_cast<unsigned char>(font_sdf_onedge_value), font_sdf_pixel_dist_scale, &width, &height, &xoff, &yoff);

    if (bitmap != nullptr) {
      glyph.width = width;
      glyph.height = height;
      glyph.xoff = xoff;
      glyph.yoff = yoff;
      glyph.bitmap.assign(bitmap, bitmap + static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

      stbtt_FreeSDF(bitmap, nullptr);

      max_width = std::max(max_width, width);
      max_height = std::max(max_height, height);
    }
  }

  const auto columns = static_cast<std::int32_t>(std::ceil(std::sqrt(static_cast<std::float_t>(font_codepoint_count))));
  const auto rows = (static_cast<std::int32_t>(font_codepoint_count) + columns - 1) / columns;

  const auto atlas_width = static_cast<std::uint32_t>(columns * max_width);
  const auto atlas_height = static_cast<std::uint32_t>(rows * max_height);

  auto atlas = std::vector<std::uint8_t>(static_cast<std::size_t>(atlas_width) * static_cast<std::size_t>(atlas_height), std::uint8_t{0u});

  auto records = std::vector<font_glyph_record>(font_codepoint_count);

  for (auto index = std::uint32_t{0u}; index < font_codepoint_count; ++index) {
    const auto& glyph = baked[index];

    auto& record = records[index];
    record.advance = glyph.advance / font_reference_size;

    if (glyph.width == 0 || glyph.height == 0) {
      record.uv_x0 = 0.0f;
      record.uv_y0 = 0.0f;
      record.uv_x1 = 0.0f;
      record.uv_y1 = 0.0f;
      record.width = 0.0f;
      record.height = 0.0f;
      record.bearing_x = 0.0f;
      record.bearing_y = 0.0f;
      continue;
    }

    const auto column = static_cast<std::int32_t>(index) % columns;
    const auto row = static_cast<std::int32_t>(index) / columns;

    const auto origin_x = static_cast<std::uint32_t>(column * max_width);
    const auto origin_y = static_cast<std::uint32_t>(row * max_height);

    for (auto y = std::int32_t{0}; y < glyph.height; ++y) {
      const auto destination_offset = (origin_y + static_cast<std::uint32_t>(y)) * atlas_width + origin_x;
      const auto source_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(glyph.width);

      std::copy_n(glyph.bitmap.begin() + static_cast<std::ptrdiff_t>(source_offset), glyph.width, atlas.begin() + static_cast<std::ptrdiff_t>(destination_offset));
    }

    record.uv_x0 = static_cast<std::float_t>(origin_x) / static_cast<std::float_t>(atlas_width);
    record.uv_y0 = static_cast<std::float_t>(origin_y) / static_cast<std::float_t>(atlas_height);
    record.uv_x1 = static_cast<std::float_t>(origin_x + static_cast<std::uint32_t>(glyph.width)) / static_cast<std::float_t>(atlas_width);
    record.uv_y1 = static_cast<std::float_t>(origin_y + static_cast<std::uint32_t>(glyph.height)) / static_cast<std::float_t>(atlas_height);
    record.width = static_cast<std::float_t>(glyph.width) / font_reference_size;
    record.height = static_cast<std::float_t>(glyph.height) / font_reference_size;
    record.bearing_x = static_cast<std::float_t>(glyph.xoff) / font_reference_size;
    record.bearing_y = static_cast<std::float_t>(glyph.yoff) / font_reference_size;
  }

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    return false;
  }

  const auto header = font_header{
    font_magic,
    font_cook_version,
    atlas_width,
    atlas_height,
    font_codepoint_count,
    font_first_codepoint,
    (static_cast<std::float_t>(ascent_units - descent_units + line_gap_units) * scale) / font_reference_size,
    (static_cast<std::float_t>(ascent_units) * scale) / font_reference_size,
    (static_cast<std::float_t>(descent_units) * scale) / font_reference_size,
    static_cast<std::uint32_t>(atlas.size())
  };

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(records.data()), static_cast<std::streamsize>(records.size() * sizeof(font_glyph_record)));
  out.write(reinterpret_cast<const char*>(atlas.data()), static_cast<std::streamsize>(atlas.size()));

  utility::logger<"assets">::debug("Cooked font '{}' -> '{}' ({}x{} atlas, {} glyphs)", source.generic_string(), cooked.generic_string(), atlas_width, atlas_height, font_codepoint_count);

  return true;
}

auto asset_cooker::_load_cooked_font(const std::filesystem::path& cooked, cooked_font_data& data) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = font_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != font_magic || header.version != font_cook_version) {
    return false; // missing / corrupt / stale format -> caller recooks
  }

  auto records = std::vector<font_glyph_record>(header.glyph_count);
  in.read(reinterpret_cast<char*>(records.data()), static_cast<std::streamsize>(records.size() * sizeof(font_glyph_record)));

  if (!in) {
    return false;
  }

  data.atlas.pixels.resize(header.atlas_data_size);
  in.read(reinterpret_cast<char*>(data.atlas.pixels.data()), static_cast<std::streamsize>(header.atlas_data_size));

  if (!in) {
    return false;
  }

  data.atlas.width = header.atlas_width;
  data.atlas.height = header.atlas_height;
  data.first_codepoint = header.first_codepoint;
  data.line_height = header.line_height;
  data.ascent = header.ascent;
  data.descent = header.descent;

  data.glyphs.clear();
  data.glyphs.reserve(records.size());

  for (const auto& record : records) {
    data.glyphs.push_back(font::glyph{
      math::vector4{record.uv_x0, record.uv_y0, record.uv_x1, record.uv_y1},
      record.width,
      record.height,
      record.bearing_x,
      record.bearing_y,
      record.advance
    });
  }

  return true;
}


} // namespace sbx::assets
