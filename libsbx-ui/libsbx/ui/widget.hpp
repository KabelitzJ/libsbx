// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_WIDGET_HPP_
#define LIBSBX_UI_WIDGET_HPP_

#include <cinttypes>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/ui/mesh.hpp>

namespace sbx::ui {

class widget {

public:

  widget(const math::vector2u& position)
  : _position{position} { }

  virtual ~widget() = default;

  virtual auto update(graphics::descriptor_handler& descriptor_handler, graphics::uniform_handler& uniform_handler, graphics::storage_handler& storage_handler) -> void = 0;

  virtual auto render(graphics::command_buffer& command_buffer, const mesh& mesh) -> void = 0;

  auto position() const noexcept -> const math::vector2u& {
    return _position;
  }

  auto set_position(const math::vector2u& position) noexcept -> void {
    _position = position;
  }

  auto id() const noexcept -> const math::uuid& {
    return _id;
  }

protected:
  
  math::vector2u _position;
  math::uuid _id;

}; // class widget

} // namespace sbx::ui

#endif // LIBSBX_UI_WIDGET_HPP_
