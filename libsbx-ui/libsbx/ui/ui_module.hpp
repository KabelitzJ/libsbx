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

#include <libsbx/ui/font.hpp>
#include <libsbx/ui/widget.hpp>
#include <libsbx/ui/container.hpp>
#include <libsbx/ui/layouts/absolute_layout.hpp>

namespace sbx::ui {

class ui_module : public core::module<ui_module> {

  inline static const auto is_registered = register_module(stage::rendering, dependencies<graphics::graphics_module>{});

public:

  ui_module()
  : _root_container{std::make_unique<absolute_layout>()} { }

  ~ui_module() override {

  }

  auto update() -> void override {

  }

  auto container() noexcept -> ui::container& {
    return _root_container;
  }

private:

  ui::container _root_container;

}; // class ui_module

} // namespace sbx::ui

#endif // LIBSBX_UI_UI_MODULE_HPP_
