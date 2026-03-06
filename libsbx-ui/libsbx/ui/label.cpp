// SPDX-License-Identifier: MIT
#include <libsbx/ui/label.hpp>

namespace sbx::ui {

auto label::set_text(const std::string& value) -> void {
  if (_text != value) {
    _text = value;
    _is_dirty = true;
  }
}

[[nodiscard]] auto label::get_text() const -> const std::string& {
  return _text;
}

auto label::set_font(const font& value) -> void {
  _font = &value;
  _is_dirty = true;
}

auto label::set_font_size(std::float_t value) -> void {
  if (_font_size != value) {
    _font_size = value;
    _is_dirty = true;
  }
}

auto label::submit(const math::vector2& screen_size) -> void {
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

auto label::_ensure_layout() -> void {
  if (!_is_dirty) {
    return;
  }

  _cached_quads.clear();

  if (!_font) {
    _is_dirty = false;
    return;
  }

  auto cursor_x = 0.0f;

  for (const auto character : _text) {
    const auto glyph = _font->find_glyph(static_cast<std::uint32_t>(character));

    if (!glyph) {
      continue;
    }

    const auto x = (cursor_x + glyph->plane_left) * _font_size;
    const auto y = glyph->plane_bottom * _font_size;
    const auto w = (glyph->plane_right - glyph->plane_left) * _font_size;
    const auto h = (glyph->plane_top - glyph->plane_bottom) * _font_size;

    if (w > 0.0f && h > 0.0f) {
      _cached_quads.push_back(text_quad{
        .position = {x, y},
        .size = {w, h},
        .uv_min = {glyph->uv_left, glyph->uv_bottom},
        .uv_max = {glyph->uv_right, glyph->uv_top}
      });
    }

    cursor_x += glyph->advance;
  }

  _is_dirty = false;
}

} // namespace sbx::ui