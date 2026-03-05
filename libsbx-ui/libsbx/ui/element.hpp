// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_ELEMENT_HPP_
#define LIBSBX_UI_ELEMENT_HPP_

#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <limits>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/images/image.hpp>

#include <libsbx/ui/font.hpp>

namespace sbx::ui {

struct rect {
  float x{0.0f};
  float y{0.0f};
  float width{0.0f};
  float height{0.0f};

  [[nodiscard]] auto contains(const math::vector2& point) const -> bool {
    return point.x() >= x 
        && point.x() <= x + width 
        && point.y() >= y 
        && point.y() <= y + height;
  }
}; // struct rect

struct element_id {
  std::uint32_t value{null_value};

  inline static constexpr auto null_value = std::numeric_limits<std::uint32_t>::max();

  [[nodiscard]] auto is_valid() const -> bool {
    return value != null_value;
  }

  auto operator==(const element_id& other) const -> bool = default;
}; // struct element_id

inline constexpr auto null_element = element_id{};

struct text_quad {
  math::vector2 position;
  math::vector2 size;
  math::vector2 uv_min;
  math::vector2 uv_max;
}; // struct text_quad

struct text_data {
  std::string text{};
  float font_size{16.0f};
  const font* font_ref{nullptr};

  std::vector<text_quad> cached_quads;
  std::string cached_text{};
  float cached_font_size{0.0f};

  auto ensure_layout() -> void {
    if (text == cached_text && font_size == cached_font_size) {
      return;
    }

    cached_quads.clear();

    if (!font_ref) {
      return;
    }

    auto cursor_x = 0.0f;

    for (const auto ch : text) {
      const auto* g = font_ref->find_glyph(static_cast<std::uint32_t>(ch));

      if (!g) {
        continue;
      }

      const auto x = (cursor_x + g->plane_left) * font_size;
      const auto y = g->plane_bottom * font_size;
      const auto w = (g->plane_right - g->plane_left) * font_size;
      const auto h = (g->plane_top - g->plane_bottom) * font_size;

      cached_quads.push_back(text_quad{
        .position = {x, y},
        .size = {w, h},
        .uv_min = {g->uv_left, g->uv_bottom},
        .uv_max = {g->uv_right, g->uv_top}
      });

      cursor_x += g->advance;
    }

    cached_text = text;
    cached_font_size = font_size;
  }
}; // struct text_data

struct element {
  math::vector2 anchor_min{0.0f, 0.0f};
  math::vector2 anchor_max{0.0f, 0.0f};
  math::vector2 offset_min{0.0f, 0.0f};
  math::vector2 offset_max{0.0f, 0.0f};
  math::vector2 pivot{0.5f, 0.5f};

  graphics::image2d_handle image{};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::int32_t sort_order{0};
  bool is_enabled{true};
  bool is_visible{true};
  bool is_interactive{false};

  element_id parent{};
  element_id first_child{};
  element_id next_sibling{};

  std::variant<std::monostate, text_data> data{};

  rect computed_rect{};
  bool is_alive{false};
}; // struct element

} // namespace sbx::ui

#endif // LIBSBX_UI_ELEMENT_HPP_
