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

  inline static const auto is_registered = register_module(stage::normal, dependencies<graphics::graphics_module, sprites::sprites_module, devices::devices_module>{});

public:

  ui_module() = default;

  ~ui_module() override = default;

  auto update() -> void override;

private:

  bool _was_mouse_down{false};

}; // class ui_module

} // namespace sbx::ui

#endif // LIBSBX_UI_UI_MODULE_HPP_
