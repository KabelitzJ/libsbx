// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_LABEL_HPP_
#define LIBSBX_UI_LABEL_HPP_

#include <string>
#include <memory>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/math/color.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/ui/widget.hpp>
#include <libsbx/ui/font.hpp>
#include <libsbx/ui/mesh.hpp>

namespace sbx::ui {

class label : public widget {

public:

  label(const std::string& text, const math::vector2u& position, memory::observer_ptr<const font> font, std::float_t scale = 1.0f, const math::color& color = math::color::white())
  : widget{position},
    _text{text},
    _font{font},
    _scale{scale},
    _color{color},
    _is_dirty{true},
    _glyph_data{} { }

  ~label() override = default;

  auto set_text(const std::string& text) -> void {
    _is_dirty = text != _text;
    _text = text;
  }

  auto update(graphics::descriptor_handler& descriptor_handler, graphics::uniform_handler& uniform_handler, graphics::storage_handler& storage_handler) -> void override {
    uniform_handler.push("color", _color);

    if (_is_dirty) {
      _recalculate_glyph_data();
    }

    storage_handler.push(std::span<const glyph_data>{_glyph_data.data(), _glyph_data.size()});

    auto& atlas = _font->atlas();

    descriptor_handler.push("atlas", atlas.image());
    descriptor_handler.push("uniform_object", uniform_handler);
    descriptor_handler.push("buffer_glyphs", storage_handler);
  }

  auto render(graphics::command_buffer& command_buffer, const mesh& mesh) -> void override {
    mesh.bind(command_buffer);
    mesh.render(command_buffer, static_cast<std::uint32_t>(_glyph_data.size()));
  }

private:

  auto _recalculate_glyph_data() -> void {
    _glyph_data.clear();

    auto position_x = static_cast<std::float_t>(_position.x());
    auto position_y = static_cast<std::float_t>(_position.y());

    for (const auto character : _text) {
      auto data = glyph_data{};

      const auto& glyph = _font->glyph(character);

      auto x = position_x + static_cast<std::float_t>(glyph.bearing.x()) * _scale;
      auto y = position_y - static_cast<std::float_t>(glyph.size.y() - glyph.bearing.y()) * _scale;

      auto w = static_cast<std::float_t>(glyph.size.x()) * _scale;
      auto h = static_cast<std::float_t>(glyph.size.y()) * _scale;

      data.offset = math::vector2{x, y};
      data.size = math::vector2{w, h};

      data.uv_offset = glyph.uv_position;
      data.uv_size = glyph.uv_size;

      _glyph_data.push_back(data);

      position_x += static_cast<std::float_t>(glyph.advance) * _scale;
    }

    _is_dirty = false;
  }

  struct glyph_data {
    alignas(8) math::vector2 offset;
    alignas(8) math::vector2 size;
    alignas(8) math::vector2 uv_offset;
    alignas(8) math::vector2 uv_size;
  }; // struct glyph_data

  std::string _text;
  memory::observer_ptr<const font> _font;
  std::float_t _scale;
  math::color _color;
  bool _is_dirty;
  std::vector<glyph_data> _glyph_data;

}; // class label

} // namespace sbx::ui

#endif // LIBSBX_UI_LABEL_HPP_
