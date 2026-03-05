// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_LABEL_HPP_
#define LIBSBX_UI_LABEL_HPP_

#include <string>
#include <vector>

#include <libsbx/math/vector2.hpp>

#include <libsbx/ui/element.hpp>
#include <libsbx/ui/font.hpp>

namespace sbx::ui {

class label : public element {

public:

  label() = default;

  ~label() override = default;

  auto set_text(const std::string& value) -> void {
    if (_text != value) {
      _text = value;
      _is_dirty = true;
    }
  }

  [[nodiscard]] auto get_text() const -> const std::string& {
    return _text;
  }

  auto set_font(const font& value) -> void {
    _font = &value;
    _is_dirty = true;
  }

  auto set_font_size(std::float_t value) -> void {
    if (_font_size != value) {
      _font_size = value;
      _is_dirty = true;
    }
  }

protected:

  auto submit(const math::vector2& screen_size) -> void override {
    auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

    if (!_font) {
      return;
    }

    _ensure_layout();

    if (_cached_quads.empty()) {
      return;
    }

    const auto atlas_index = sprites_module.register_image(_font->atlas);

    const auto& r = computed_rectangle();

    const auto origin_x = r.x;
    const auto origin_y = r.y + r.height * 0.5f - _font_size * 0.5f;

    for (const auto& quad : _cached_quads) {
      const auto glyph_x = origin_x + quad.position.x();
      const auto glyph_y = origin_y + quad.position.y();

      submit_quad(screen_size, {glyph_x, glyph_y}, quad.size, {0.0f, 0.0f}, color, atlas_index, sort_order, quad.uv_min, quad.uv_max, sprites::sprite_batch::flag_msdf_text, _font->sdf_px_range);
    }
  }

private:

  struct text_quad {
    math::vector2 position;
    math::vector2 size;
    math::vector2 uv_min;
    math::vector2 uv_max;
  }; // struct text_quad

  auto _ensure_layout() -> void {
    if (!_is_dirty) {
      return;
    }

    _cached_quads.clear();

    if (!_font) {
      _is_dirty = false;
      return;
    }

    auto cursor_x = 0.0f;

    for (const auto ch : _text) {
      const auto* g = _font->find_glyph(static_cast<std::uint32_t>(ch));

      if (!g) {
        continue;
      }

      const auto x = (cursor_x + g->plane_left) * _font_size;
      const auto y = g->plane_bottom * _font_size;
      const auto w = (g->plane_right - g->plane_left) * _font_size;
      const auto h = (g->plane_top - g->plane_bottom) * _font_size;

      if (w > 0.0f && h > 0.0f) {
        _cached_quads.push_back(text_quad{
          .position = {x, y},
          .size = {w, h},
          .uv_min = {g->uv_left, g->uv_bottom},
          .uv_max = {g->uv_right, g->uv_top}
        });
      }

      cursor_x += g->advance;
    }

    _is_dirty = false;
  }

  const font* _font{nullptr};
  std::string _text{};
  std::float_t _font_size{16.0f};
  std::vector<text_quad> _cached_quads;
  bool _is_dirty{true};

}; // class label

} // namespace sbx::ui

#endif // LIBSBX_UI_LABEL_HPP_
