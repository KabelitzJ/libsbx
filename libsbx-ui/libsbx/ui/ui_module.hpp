// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_UI_MODULE_HPP_
#define LIBSBX_UI_UI_MODULE_HPP_

#include <libsbx/core/module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/ui/canvas.hpp>

namespace sbx::ui {

class ui_module : public core::module<ui_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<
    graphics::graphics_module,
    sprites::sprites_module,
    devices::devices_module
  >{});

public:

  ui_module() = default;

  ~ui_module() override = default;

  auto update() -> void override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

    const auto screen_size = graphics_module.viewport();

    const auto mouse_position = devices::input::mouse_position();
    const auto is_down = devices::input::is_mouse_button_down(devices::mouse_button::left);
    const auto was_down = _was_mouse_down;

    auto& scene = scenes_module.scene();

    auto canvas_query = scene.query<ui::canvas>();

    for (auto&& [node, canvas] : canvas_query.each()) {
      canvas.update(screen_size);
      canvas.process_input(mouse_position, is_down, was_down);
      canvas.submit(sprites_module);
    }

    _was_mouse_down = is_down;
  }

private:

  bool _was_mouse_down{false};

}; // class ui_module

} // namespace sbx::ui

#endif // LIBSBX_UI_UI_MODULE_HPP_
