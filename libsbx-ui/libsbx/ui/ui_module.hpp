// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_UI_MODULE_HPP_
#define LIBSBX_UI_UI_MODULE_HPP_

#include <unordered_map>
#include <filesystem>
#include <type_traits>

#include <fmt/format.h>

#include <freetype/freetype.h>

#include <libsbx/core/module.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/ui/element.hpp>
#include <libsbx/ui/canvas.hpp>

namespace sbx::ui {

class ui_module : public core::module<ui_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<graphics::graphics_module, sprites::sprites_module, devices::devices_module>{});

public:

  ui_module() {

  }

  ~ui_module() override {

  }

  auto update() -> void override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

    const auto screen_size = graphics_module.viewport();

    auto& scene = scenes_module.scene();

    auto canvas_query = scene.query<ui::canvas>();

    for (auto&& [node, canvas] : canvas_query.each()) {
      canvas.update(screen_size);
      canvas.submit(sprites_module);
    }
  }

private:



}; // class ui_module

} // namespace sbx::ui

#endif // LIBSBX_UI_UI_MODULE_HPP_
