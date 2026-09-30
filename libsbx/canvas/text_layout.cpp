// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/canvas/text_layout.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace sbx::canvas {

auto next_codepoint(const std::string& text, std::size_t& i) -> std::uint32_t {
  const auto lead = static_cast<std::uint8_t>(text[i++]);

  if (lead < 0x80u) {
    return lead;
  }

  const auto length = lead >= 0xf0u ? 3u : lead >= 0xe0u ? 2u : lead >= 0xc0u ? 1u : 0u;

  if (length == 0u || i + length > text.size()) {
    return 0xfffdu;
  }

  auto codepoint = static_cast<std::uint32_t>(lead & (0x3fu >> length));

  for (auto k = 0u; k < length; ++k) {
    const auto byte = static_cast<std::uint8_t>(text[i + k]);

    if ((byte & 0xc0u) != 0x80u) {
      return 0xfffdu;
    }

    codepoint = (codepoint << 6u) | (byte & 0x3fu);
  }

  i += length;
  return codepoint;
}

auto shape_text(const ui_text& text, const resolved_rect& rect) -> std::vector<text_glyph> {
  auto glyphs = std::vector<text_glyph>{};

  if (!text.font || !text.font->is_valid() || text.text.empty()) {
    return glyphs;
  }

  const auto& font = *text.font;
  const auto scale = text.font_size;
  const auto line_height = font.line_height() * scale * text.line_spacing;
  const auto texture_index = font.atlas()->index();

  auto lines = std::vector<std::pair<std::size_t, std::size_t>>{};

  auto line_start = std::size_t{0u};

  for (auto i = std::size_t{0u}; i <= text.text.size(); ++i) {
    if (i == text.text.size() || text.text[i] == '\n') {
      lines.emplace_back(line_start, i);
      line_start = i + 1u;
    }
  }

  auto line_widths = std::vector<std::float_t>(lines.size(), 0.0f);

  for (auto line_index = std::size_t{0u}; line_index < lines.size(); ++line_index) {
    auto width = 0.0f;

    for (auto i = lines[line_index].first; i < lines[line_index].second;) {
      const auto codepoint = next_codepoint(text.text, i);

      if (const auto glyph = font.glyph_for(codepoint)) {
        width += glyph->advance * scale;
      }
    }

    line_widths[line_index] = width;
  }

  const auto block_height = static_cast<std::float_t>(lines.size()) * line_height;

  auto block_origin_y = rect.position.y();

  if (text.vertical_align == text_align::center) {
    block_origin_y += (rect.size.y() - block_height) * 0.5f;
  } else if (text.vertical_align == text_align::end) {
    block_origin_y += rect.size.y() - block_height;
  }

  for (auto line_index = std::size_t{0u}; line_index < lines.size(); ++line_index) {
    auto pen_x = rect.position.x();

    if (text.horizontal_align == text_align::center) {
      pen_x += (rect.size.x() - line_widths[line_index]) * 0.5f;
    } else if (text.horizontal_align == text_align::end) {
      pen_x += rect.size.x() - line_widths[line_index];
    }

    const auto baseline_y = block_origin_y + static_cast<std::float_t>(line_index) * line_height + font.ascent() * scale;

    for (auto i = lines[line_index].first; i < lines[line_index].second;) {
      const auto codepoint = next_codepoint(text.text, i);
      const auto glyph = font.glyph_for(codepoint);

      if (!glyph) {
        continue;
      }

      if (glyph->width > 0.0f && glyph->height > 0.0f) {
        const auto quad_position = math::vector2{pen_x + glyph->bearing_x * scale, baseline_y + glyph->bearing_y * scale};
        const auto quad_size = math::vector2{glyph->width * scale, glyph->height * scale};

        glyphs.push_back(text_glyph{resolved_rect{quad_position, quad_size}, glyph->uv_rect, texture_index});
      }

      pen_x += glyph->advance * scale;
    }
  }

  return glyphs;
}

} // namespace sbx::canvas
